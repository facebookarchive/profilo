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

#include "valgrind_settings.h"

namespace art {
namespace gc {
namespace space {

namespace valgrind_details {

template <size_t kValgrindRedZoneBytes, bool kUseObjSizeForUsable>
inline mirror::Object* AdjustForValgrind(void* obj_with_rdz, size_t num_bytes,
                                         size_t bytes_allocated, size_t usable_size,
                                         size_t bytes_tl_bulk_allocated,
                                         size_t* bytes_allocated_out, size_t* usable_size_out,
                                         size_t* bytes_tl_bulk_allocated_out) {
  if (bytes_allocated_out != nullptr) {
    *bytes_allocated_out = bytes_allocated;
  }
  if (bytes_tl_bulk_allocated_out != nullptr) {
    *bytes_tl_bulk_allocated_out = bytes_tl_bulk_allocated;
  }

  // This cuts over-provision and is a trade-off between testing the over-provisioning code paths
  // vs checking overflows in the regular paths.
  if (usable_size_out != nullptr) {
    if (kUseObjSizeForUsable) {
      *usable_size_out = num_bytes;
    } else {
      *usable_size_out = usable_size - 2 * kValgrindRedZoneBytes;
    }
  }

  // Left redzone.
  VALGRIND_MAKE_MEM_NOACCESS(obj_with_rdz, kValgrindRedZoneBytes);

  // Make requested memory readable.
  // (If the allocator assumes memory is zeroed out, we might get UNDEFINED warnings, so make
  //  everything DEFINED initially.)
  mirror::Object* result = reinterpret_cast<mirror::Object*>(
      reinterpret_cast<uint8_t*>(obj_with_rdz) + kValgrindRedZoneBytes);
  VALGRIND_MAKE_MEM_DEFINED(result, num_bytes);

  // Right redzone. Assumes that if bytes_allocated > usable_size, then the difference is
  // management data at the upper end, and for simplicity we will not protect that.
  // At the moment, this fits RosAlloc (no management data in a slot, usable_size == alloc_size)
  // and DlMalloc (allocation_size = (usable_size == num_bytes) + 4, 4 is management)
  VALGRIND_MAKE_MEM_NOACCESS(reinterpret_cast<uint8_t*>(result) + num_bytes,
                             usable_size - (num_bytes + kValgrindRedZoneBytes));

  return result;
}

inline size_t GetObjSizeNoThreadSafety(mirror::Object* obj) NO_THREAD_SAFETY_ANALYSIS {
  return obj->SizeOf<kVerifyNone>();
}

}  // namespace valgrind_details

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
mirror::Object*
ValgrindMallocSpace<S,
                    kValgrindRedZoneBytes,
                    kAdjustForRedzoneInAllocSize,
                    kUseObjSizeForUsable>::AllocWithGrowth(
    Thread* self, size_t num_bytes, size_t* bytes_allocated_out, size_t* usable_size_out,
    size_t* bytes_tl_bulk_allocated_out) {
  size_t bytes_allocated;
  size_t usable_size;
  size_t bytes_tl_bulk_allocated;
  void* obj_with_rdz = S::AllocWithGrowth(self, num_bytes + 2 * kValgrindRedZoneBytes,
                                          &bytes_allocated, &usable_size,
                                          &bytes_tl_bulk_allocated);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }

  return valgrind_details::AdjustForValgrind<kValgrindRedZoneBytes, kUseObjSizeForUsable>(
      obj_with_rdz, num_bytes,
      bytes_allocated, usable_size,
      bytes_tl_bulk_allocated,
      bytes_allocated_out,
      usable_size_out,
      bytes_tl_bulk_allocated_out);
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
mirror::Object* ValgrindMallocSpace<S,
                                    kValgrindRedZoneBytes,
                                    kAdjustForRedzoneInAllocSize,
                                    kUseObjSizeForUsable>::Alloc(
    Thread* self, size_t num_bytes, size_t* bytes_allocated_out, size_t* usable_size_out,
    size_t* bytes_tl_bulk_allocated_out) {
  size_t bytes_allocated;
  size_t usable_size;
  size_t bytes_tl_bulk_allocated;
  void* obj_with_rdz = S::Alloc(self, num_bytes + 2 * kValgrindRedZoneBytes,
                                &bytes_allocated, &usable_size, &bytes_tl_bulk_allocated);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }

  return valgrind_details::AdjustForValgrind<kValgrindRedZoneBytes,
                                             kUseObjSizeForUsable>(obj_with_rdz, num_bytes,
                                                                   bytes_allocated, usable_size,
                                                                   bytes_tl_bulk_allocated,
                                                                   bytes_allocated_out,
                                                                   usable_size_out,
                                                                   bytes_tl_bulk_allocated_out);
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
mirror::Object* ValgrindMallocSpace<S,
                                    kValgrindRedZoneBytes,
                                    kAdjustForRedzoneInAllocSize,
                                    kUseObjSizeForUsable>::AllocThreadUnsafe(
    Thread* self, size_t num_bytes, size_t* bytes_allocated_out, size_t* usable_size_out,
    size_t* bytes_tl_bulk_allocated_out) {
  size_t bytes_allocated;
  size_t usable_size;
  size_t bytes_tl_bulk_allocated;
  void* obj_with_rdz = S::AllocThreadUnsafe(self, num_bytes + 2 * kValgrindRedZoneBytes,
                                            &bytes_allocated, &usable_size,
                                            &bytes_tl_bulk_allocated);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }

  return valgrind_details::AdjustForValgrind<kValgrindRedZoneBytes, kUseObjSizeForUsable>(
      obj_with_rdz, num_bytes,
      bytes_allocated, usable_size,
      bytes_tl_bulk_allocated,
      bytes_allocated_out,
      usable_size_out,
      bytes_tl_bulk_allocated_out);
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t ValgrindMallocSpace<S,
                           kValgrindRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::AllocationSize(
    mirror::Object* obj, size_t* usable_size) {
  size_t result = S::AllocationSize(reinterpret_cast<mirror::Object*>(
      reinterpret_cast<uint8_t*>(obj) - (kAdjustForRedzoneInAllocSize ? kValgrindRedZoneBytes : 0)),
      usable_size);
  if (usable_size != nullptr) {
    if (kUseObjSizeForUsable) {
      *usable_size = valgrind_details::GetObjSizeNoThreadSafety(obj);
    } else {
      *usable_size = *usable_size - 2 * kValgrindRedZoneBytes;
    }
  }
  return result;
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t ValgrindMallocSpace<S,
                           kValgrindRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::Free(
    Thread* self, mirror::Object* ptr) {
  void* obj_after_rdz = reinterpret_cast<void*>(ptr);
  uint8_t* obj_with_rdz = reinterpret_cast<uint8_t*>(obj_after_rdz) - kValgrindRedZoneBytes;
  // Make redzones undefined.
  size_t usable_size;
  size_t allocation_size = AllocationSize(ptr, &usable_size);

  // Unprotect the allocation.
  // Use the obj-size-for-usable flag to determine whether usable_size is the more important one,
  // e.g., whether there's data in the allocation_size (and usable_size can't be trusted).
  if (kUseObjSizeForUsable) {
    VALGRIND_MAKE_MEM_UNDEFINED(obj_with_rdz, allocation_size);
  } else {
    VALGRIND_MAKE_MEM_UNDEFINED(obj_with_rdz, usable_size + 2 * kValgrindRedZoneBytes);
  }

  return S::Free(self, reinterpret_cast<mirror::Object*>(obj_with_rdz));
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t ValgrindMallocSpace<S,
                           kValgrindRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::FreeList(
    Thread* self, size_t num_ptrs, mirror::Object** ptrs) {
  size_t freed = 0;
  for (size_t i = 0; i < num_ptrs; i++) {
    freed += Free(self, ptrs[i]);
    ptrs[i] = nullptr;
  }
  return freed;
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
template <typename... Params>
ValgrindMallocSpace<S,
                    kValgrindRedZoneBytes,
                    kAdjustForRedzoneInAllocSize,
                    kUseObjSizeForUsable>::ValgrindMallocSpace(
    MemMap* mem_map, size_t initial_size, Params... params) : S(mem_map, initial_size, params...) {
  VALGRIND_MAKE_MEM_UNDEFINED(mem_map->Begin() + initial_size,
                              mem_map->Size() - initial_size);
}

template <typename S,
          size_t kValgrindRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t ValgrindMallocSpace<S,
                           kValgrindRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::MaxBytesBulkAllocatedFor(size_t num_bytes) {
  return S::MaxBytesBulkAllocatedFor(num_bytes + 2 * kValgrindRedZoneBytes);
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_VALGRIND_MALLOC_SPACE_INL_H_
