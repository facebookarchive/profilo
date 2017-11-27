/*
 * Copyright 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_JIT_CODE_CACHE_H_
#define ART_RUNTIME_JIT_JIT_CODE_CACHE_H_

#include "instrumentation.h"

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "jni.h"
#include "oat_file.h"
#include "object_callbacks.h"
#include "safe_map.h"
#include "thread_pool.h"

namespace art {

class ArtMethod;
class CompiledMethod;
class CompilerCallbacks;

namespace jit {

class JitInstrumentationCache;

class JitCodeCache {
 public:
  static constexpr size_t kMaxCapacity = 1 * GB;
  static constexpr size_t kDefaultCapacity = 2 * MB;

  // Create the code cache with a code + data capacity equal to "capacity", error message is passed
  // in the out arg error_msg.
  static JitCodeCache* Create(size_t capacity, std::string* error_msg);

  const uint8_t* CodeCachePtr() const {
    return code_cache_ptr_;
  }

  size_t CodeCacheSize() const {
    return code_cache_ptr_ - code_cache_begin_;
  }

  size_t CodeCacheRemain() const {
    return code_cache_end_ - code_cache_ptr_;
  }

  const uint8_t* DataCachePtr() const {
    return data_cache_ptr_;
  }

  size_t DataCacheSize() const {
    return data_cache_ptr_ - data_cache_begin_;
  }

  size_t DataCacheRemain() const {
    return data_cache_end_ - data_cache_ptr_;
  }

  size_t NumMethods() const {
    return num_methods_;
  }

  // Return true if the code cache contains the code pointer which si the entrypoint of the method.
  bool ContainsMethod(ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Return true if the code cache contains a code ptr.
  bool ContainsCodePtr(const void* ptr) const;

  // Reserve a region of code of size at least "size". Returns null if there is no more room.
  uint8_t* ReserveCode(Thread* self, size_t size) LOCKS_EXCLUDED(lock_);

  // Add a data array of size (end - begin) with the associated contents, returns null if there
  // is no more room.
  uint8_t* AddDataArray(Thread* self, const uint8_t* begin, const uint8_t* end)
      LOCKS_EXCLUDED(lock_);

  // Get code for a method, returns null if it is not in the jit cache.
  const void* GetCodeFor(ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);

  // Save the compiled code for a method so that GetCodeFor(method) will return old_code_ptr if the
  // entrypoint isn't within the cache.
  void SaveCompiledCode(ArtMethod* method, const void* old_code_ptr)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);

 private:
  // Takes ownership of code_mem_map.
  explicit JitCodeCache(MemMap* code_mem_map);

  // Unimplemented, TODO: Determine if it is necessary.
  void FlushInstructionCache();

  // Lock which guards.
  Mutex lock_;
  // Mem map which holds code and data. We do this since we need to have 32 bit offsets from method
  // headers in code cache which point to things in the data cache. If the maps are more than 4GB
  // apart, having multiple maps wouldn't work.
  std::unique_ptr<MemMap> mem_map_;
  // Code cache section.
  uint8_t* code_cache_ptr_;
  const uint8_t* code_cache_begin_;
  const uint8_t* code_cache_end_;
  // Data cache section.
  uint8_t* data_cache_ptr_;
  const uint8_t* data_cache_begin_;
  const uint8_t* data_cache_end_;
  size_t num_methods_;
  // This map holds code for methods if they were deoptimized by the instrumentation stubs. This is
  // required since we have to implement ClassLinker::GetQuickOatCodeFor for walking stacks.
  SafeMap<ArtMethod*, const void*> method_code_map_ GUARDED_BY(lock_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JitCodeCache);
};


}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_CODE_CACHE_H_
