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

#ifndef ART_RUNTIME_GC_SPACE_VALGRIND_MALLOC_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_VALGRIND_MALLOC_SPACE_INL_H_

#include "valgrind_malloc_space.h"

#include <memcheck/memcheck.h>

namespace art {
namespace gc {
namespace space {

// Number of bytes to use as a red zone (rdz). A red zone of this size will be placed before and
// after each allocation. 8 bytes provides long/double alignment.
static constexpr size_t kValgrindRedZoneBytes = 8;

template <typename S, typename A>
mirror::Object* ValgrindMallocSpace<S, A>::AllocWithGrowth(Thread* self, size_t num_bytes,
                                                           size_t* bytes_allocated,
                                                           size_t* usable_size) {
  void* obj_with_rdz = S::AllocWithGrowth(self, num_bytes + 2 * kValgrindRedZoneBytes,
                                          bytes_allocated, usable_size);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }
  mirror::Object* result = reinterpret_cast<mirror::Object*>(
      reinterpret_cast<byte*>(obj_with_rdz) + kValgrindRedZoneBytes);
  // Make redzones as no access.
  VALGRIND_MAKE_MEM_NOACCESS(obj_with_rdz, kValgrindRedZoneBytes);
  VALGRIND_MAKE_MEM_NOACCESS(reinterpret_cast<byte*>(result) + num_bytes, kValgrindRedZoneBytes);
  return result;
}

template <typename S, typename A>
mirror::Object* ValgrindMallocSpace<S, A>::Alloc(Thread* self, size_t num_bytes,
                                                 size_t* bytes_allocated,
                                                 size_t* usable_size) {
  void* obj_with_rdz = S::Alloc(self, num_bytes + 2 * kValgrindRedZoneBytes, bytes_allocated,
                                usable_size);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }
  mirror::Object* result = reinterpret_cast<mirror::Object*>(
      reinterpret_cast<byte*>(obj_with_rdz) + kValgrindRedZoneBytes);
  // Make redzones as no access.
  VALGRIND_MAKE_MEM_NOACCESS(obj_with_rdz, kValgrindRedZoneBytes);
  VALGRIND_MAKE_MEM_NOACCESS(reinterpret_cast<byte*>(result) + num_bytes, kValgrindRedZoneBytes);
  return result;
}

template <typename S, typename A>
size_t ValgrindMallocSpace<S, A>::AllocationSize(mirror::Object* obj, size_t* usable_size) {
  size_t result = S::AllocationSize(reinterpret_cast<mirror::Object*>(
      reinterpret_cast<byte*>(obj) - kValgrindRedZoneBytes), usable_size);
  return result;
}

template <typename S, typename A>
size_t ValgrindMallocSpace<S, A>::Free(Thread* self, mirror::Object* ptr) {
  void* obj_after_rdz = reinterpret_cast<void*>(ptr);
  void* obj_with_rdz = reinterpret_cast<byte*>(obj_after_rdz) - kValgrindRedZoneBytes;
  // Make redzones undefined.
  size_t usable_size = 0;
  AllocationSize(ptr, &usable_size);
  VALGRIND_MAKE_MEM_UNDEFINED(obj_with_rdz, usable_size);
  return S::Free(self, reinterpret_cast<mirror::Object*>(obj_with_rdz));
}

template <typename S, typename A>
size_t ValgrindMallocSpace<S, A>::FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) {
  size_t freed = 0;
  for (size_t i = 0; i < num_ptrs; i++) {
    freed += Free(self, ptrs[i]);
    ptrs[i] = nullptr;
  }
  return freed;
}

template <typename S, typename A>
ValgrindMallocSpace<S, A>::ValgrindMallocSpace(const std::string& name, MemMap* mem_map,
                                               A allocator, byte* begin,
                                               byte* end, byte* limit, size_t growth_limit,
                                               size_t initial_size,
                                               bool can_move_objects, size_t starting_size) :
    S(name, mem_map, allocator, begin, end, limit, growth_limit, can_move_objects, starting_size,
      initial_size) {
  VALGRIND_MAKE_MEM_UNDEFINED(mem_map->Begin() + initial_size, mem_map->Size() - initial_size);
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_VALGRIND_MALLOC_SPACE_INL_H_
