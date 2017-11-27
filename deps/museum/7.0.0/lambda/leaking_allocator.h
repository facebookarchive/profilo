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
#ifndef ART_RUNTIME_LAMBDA_LEAKING_ALLOCATOR_H_
#define ART_RUNTIME_LAMBDA_LEAKING_ALLOCATOR_H_

#include <utility>  // std::forward
#include <type_traits>  // std::aligned_storage

namespace art {
class Thread;  // forward declaration

namespace lambda {

// Temporary class to centralize all the leaking allocations.
// Allocations made through this class are never freed, but it is a placeholder
// that means that the calling code needs to be rewritten to properly:
//
// (a) Have a lifetime scoped to some other entity.
// (b) Not be allocated over and over again if it was already allocated once (immutable data).
//
// TODO: do all of the above a/b for each callsite, and delete this class.
class LeakingAllocator {
 public:
  // An opaque type which is guaranteed for:
  // * a) be large enough to hold T (e.g. for in-place new)
  // * b) be well-aligned (so that reads/writes are well-defined) to T
  // * c) strict-aliasing compatible with T*
  //
  // Nominally used to allocate memory for yet unconstructed instances of T.
  template <typename T>
  using AlignedMemoryStorage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

  // Allocate byte_size bytes worth of memory. Never freed.
  template <typename T>
  static AlignedMemoryStorage<T>* AllocateMemory(Thread* self, size_t byte_size = sizeof(T)) {
    return reinterpret_cast<AlignedMemoryStorage<T>*>(
        AllocateMemoryImpl(self, byte_size, alignof(T)));
  }

  // Make a new instance of T, flexibly sized, in-place at newly allocated memory. Never freed.
  template <typename T, typename... Args>
  static T* MakeFlexibleInstance(Thread* self, size_t byte_size, Args&&... args) {
    return new (AllocateMemory<T>(self, byte_size)) T(std::forward<Args>(args)...);
  }

  // Make a new instance of T in-place at newly allocated memory. Never freed.
  template <typename T, typename... Args>
  static T* MakeInstance(Thread* self, Args&&... args) {
    return new (AllocateMemory<T>(self, sizeof(T))) T(std::forward<Args>(args)...);
  }

 private:
  static void* AllocateMemoryImpl(Thread* self, size_t byte_size, size_t align_size);
};

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_LEAKING_ALLOCATOR_H_
