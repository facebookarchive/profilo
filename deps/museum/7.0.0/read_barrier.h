/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_READ_BARRIER_H_
#define ART_RUNTIME_READ_BARRIER_H_

#include "base/mutex.h"
#include "base/macros.h"
#include "gc_root.h"
#include "jni.h"
#include "mirror/object_reference.h"
#include "offsets.h"
#include "read_barrier_c.h"

// This is a C++ (not C) header file, separate from read_barrier_c.h
// which needs to be a C header file for asm_support.h.

namespace art {
namespace mirror {
  class Object;
  template<typename MirrorType> class HeapReference;
}  // namespace mirror
class ArtMethod;

class ReadBarrier {
 public:
  // TODO: disable thse flags for production use.
  // Enable the to-space invariant checks.
  static constexpr bool kEnableToSpaceInvariantChecks = true;
  // Enable the read barrier checks.
  static constexpr bool kEnableReadBarrierInvariantChecks = true;

  // It's up to the implementation whether the given field gets updated whereas the return value
  // must be an updated reference unless kAlwaysUpdateField is true.
  template <typename MirrorType, ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            bool kAlwaysUpdateField = false>
  ALWAYS_INLINE static MirrorType* Barrier(
      mirror::Object* obj, MemberOffset offset, mirror::HeapReference<MirrorType>* ref_addr)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // It's up to the implementation whether the given root gets updated
  // whereas the return value must be an updated reference.
  template <typename MirrorType, ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE static MirrorType* BarrierForRoot(MirrorType** root,
                                                  GcRootSource* gc_root_source = nullptr)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // It's up to the implementation whether the given root gets updated
  // whereas the return value must be an updated reference.
  template <typename MirrorType, ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE static MirrorType* BarrierForRoot(mirror::CompressedReference<MirrorType>* root,
                                                  GcRootSource* gc_root_source = nullptr)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static bool IsDuringStartup();

  // Without the holder object.
  static void AssertToSpaceInvariant(mirror::Object* ref)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    AssertToSpaceInvariant(nullptr, MemberOffset(0), ref);
  }
  // With the holder object.
  static void AssertToSpaceInvariant(mirror::Object* obj, MemberOffset offset,
                                     mirror::Object* ref)
      SHARED_REQUIRES(Locks::mutator_lock_);
  // With GcRootSource.
  static void AssertToSpaceInvariant(GcRootSource* gc_root_source, mirror::Object* ref)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // ALWAYS_INLINE on this caused a performance regression b/26744236.
  static mirror::Object* Mark(mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_);

  static mirror::Object* WhitePtr() {
    return reinterpret_cast<mirror::Object*>(white_ptr_);
  }
  static mirror::Object* GrayPtr() {
    return reinterpret_cast<mirror::Object*>(gray_ptr_);
  }
  static mirror::Object* BlackPtr() {
    return reinterpret_cast<mirror::Object*>(black_ptr_);
  }

  ALWAYS_INLINE static bool HasGrayReadBarrierPointer(mirror::Object* obj,
                                                      uintptr_t* out_rb_ptr_high_bits)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Note: These couldn't be constexpr pointers as reinterpret_cast isn't compatible with them.
  static constexpr uintptr_t white_ptr_ = 0x0;    // Not marked.
  static constexpr uintptr_t gray_ptr_ = 0x1;     // Marked, but not marked through. On mark stack.
  static constexpr uintptr_t black_ptr_ = 0x2;    // Marked through. Used for non-moving objects.
  static constexpr uintptr_t rb_ptr_mask_ = 0x3;  // The low 2 bits for white|gray|black.
};

}  // namespace art

#endif  // ART_RUNTIME_READ_BARRIER_H_
