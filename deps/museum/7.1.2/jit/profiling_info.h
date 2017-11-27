/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_JIT_PROFILING_INFO_H_
#define ART_RUNTIME_JIT_PROFILING_INFO_H_

#include <vector>

#include "base/macros.h"
#include "gc_root.h"

namespace art {

class ArtMethod;
class ProfilingInfo;

namespace jit {
class JitCodeCache;
}

namespace mirror {
class Class;
}

// Structure to store the classes seen at runtime for a specific instruction.
// Once the classes_ array is full, we consider the INVOKE to be megamorphic.
class InlineCache {
 public:
  bool IsMonomorphic() const {
    DCHECK_GE(kIndividualCacheSize, 2);
    return !classes_[0].IsNull() && classes_[1].IsNull();
  }

  bool IsMegamorphic() const {
    for (size_t i = 0; i < kIndividualCacheSize; ++i) {
      if (classes_[i].IsNull()) {
        return false;
      }
    }
    return true;
  }

  mirror::Class* GetMonomorphicType() const SHARED_REQUIRES(Locks::mutator_lock_) {
    // Note that we cannot ensure the inline cache is actually monomorphic
    // at this point, as other threads may have updated it.
    DCHECK(!classes_[0].IsNull());
    return classes_[0].Read();
  }

  bool IsUninitialized() const {
    return classes_[0].IsNull();
  }

  bool IsPolymorphic() const {
    DCHECK_GE(kIndividualCacheSize, 3);
    return !classes_[1].IsNull() && classes_[kIndividualCacheSize - 1].IsNull();
  }

  mirror::Class* GetTypeAt(size_t i) const SHARED_REQUIRES(Locks::mutator_lock_) {
    return classes_[i].Read();
  }

  static constexpr uint16_t kIndividualCacheSize = 5;

 private:
  uint32_t dex_pc_;
  GcRoot<mirror::Class> classes_[kIndividualCacheSize];

  friend class ProfilingInfo;

  DISALLOW_COPY_AND_ASSIGN(InlineCache);
};

/**
 * Profiling info for a method, created and filled by the interpreter once the
 * method is warm, and used by the compiler to drive optimizations.
 */
class ProfilingInfo {
 public:
  // Create a ProfilingInfo for 'method'. Return whether it succeeded, or if it is
  // not needed in case the method does not have virtual/interface invocations.
  static bool Create(Thread* self, ArtMethod* method, bool retry_allocation)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Add information from an executed INVOKE instruction to the profile.
  void AddInvokeInfo(uint32_t dex_pc, mirror::Class* cls)
      // Method should not be interruptible, as it manipulates the ProfilingInfo
      // which can be concurrently collected.
      REQUIRES(Roles::uninterruptible_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // NO_THREAD_SAFETY_ANALYSIS since we don't know what the callback requires.
  template<typename RootVisitorType>
  void VisitRoots(RootVisitorType& visitor) NO_THREAD_SAFETY_ANALYSIS {
    visitor.VisitRootIfNonNull(holding_class_.AddressWithoutBarrier());
    for (size_t i = 0; i < number_of_inline_caches_; ++i) {
      InlineCache* cache = &cache_[i];
      for (size_t j = 0; j < InlineCache::kIndividualCacheSize; ++j) {
        visitor.VisitRootIfNonNull(cache->classes_[j].AddressWithoutBarrier());
      }
    }
  }

  ArtMethod* GetMethod() const {
    return method_;
  }

  InlineCache* GetInlineCache(uint32_t dex_pc);

  bool IsMethodBeingCompiled(bool osr) const {
    return osr
        ? is_osr_method_being_compiled_
        : is_method_being_compiled_;
  }

  void SetIsMethodBeingCompiled(bool value, bool osr) {
    if (osr) {
      is_osr_method_being_compiled_ = value;
    } else {
      is_method_being_compiled_ = value;
    }
  }

  void SetSavedEntryPoint(const void* entry_point) {
    saved_entry_point_ = entry_point;
  }

  const void* GetSavedEntryPoint() const {
    return saved_entry_point_;
  }

  void ClearGcRootsInInlineCaches() {
    for (size_t i = 0; i < number_of_inline_caches_; ++i) {
      InlineCache* cache = &cache_[i];
      memset(&cache->classes_[0],
             0,
             InlineCache::kIndividualCacheSize * sizeof(GcRoot<mirror::Class>));
    }
  }

  void IncrementInlineUse() {
    DCHECK_NE(current_inline_uses_, std::numeric_limits<uint16_t>::max());
    current_inline_uses_++;
  }

  void DecrementInlineUse() {
    DCHECK_GT(current_inline_uses_, 0);
    current_inline_uses_--;
  }

  bool IsInUseByCompiler() const {
    return IsMethodBeingCompiled(/*osr*/ true) || IsMethodBeingCompiled(/*osr*/ false) ||
        (current_inline_uses_ > 0);
  }

 private:
  ProfilingInfo(ArtMethod* method, const std::vector<uint32_t>& entries);

  // Number of instructions we are profiling in the ArtMethod.
  const uint32_t number_of_inline_caches_;

  // Method this profiling info is for.
  ArtMethod* const method_;

  // Holding class for the method in case method is a copied method.
  GcRoot<mirror::Class> holding_class_;

  // Whether the ArtMethod is currently being compiled. This flag
  // is implicitly guarded by the JIT code cache lock.
  // TODO: Make the JIT code cache lock global.
  bool is_method_being_compiled_;
  bool is_osr_method_being_compiled_;

  // When the compiler inlines the method associated to this ProfilingInfo,
  // it updates this counter so that the GC does not try to clear the inline caches.
  uint16_t current_inline_uses_;

  // Entry point of the corresponding ArtMethod, while the JIT code cache
  // is poking for the liveness of compiled code.
  const void* saved_entry_point_;

  // Dynamically allocated array of size `number_of_inline_caches_`.
  InlineCache cache_[0];

  friend class jit::JitCodeCache;

  DISALLOW_COPY_AND_ASSIGN(ProfilingInfo);
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_PROFILING_INFO_H_
