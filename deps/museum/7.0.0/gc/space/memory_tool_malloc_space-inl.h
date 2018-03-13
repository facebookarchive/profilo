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

#ifndef ART_RUNTIME_GC_SPACE_MEMORY_TOOL_MALLOC_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_MEMORY_TOOL_MALLOC_SPACE_INL_H_

#include "base/memory_tool.h"
#include "memory_tool_malloc_space.h"
#include "memory_tool_settings.h"

namespace art {
namespace gc {
namespace space {

namespace memory_tool_details {

template <size_t kMemoryToolRedZoneBytes, bool kUseObjSizeForUsable>
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
      *usable_size_out = usable_size - 2 * kMemoryToolRedZoneBytes;
    }
  }

  // Left redzone.
  MEMORY_TOOL_MAKE_NOACCESS(obj_with_rdz, kMemoryToolRedZoneBytes);

  // Make requested memory readable.
  // (If the allocator assumes memory is zeroed out, we might get UNDEFINED warnings, so make
  //  everything DEFINED initially.)
  mirror::Object* result = reinterpret_cast<mirror::Object*>(
      reinterpret_cast<uint8_t*>(obj_with_rdz) + kMemoryToolRedZoneBytes);
  MEMORY_TOOL_MAKE_DEFINED(result, num_bytes);

  // Right redzone. Assumes that if bytes_allocated > usable_size, then the difference is
  // management data at the upper end, and for simplicity we will not protect that.
  // At the moment, this fits RosAlloc (no management data in a slot, usable_size == alloc_size)
  // and DlMalloc (allocation_size = (usable_size == num_bytes) + 4, 4 is management)
  MEMORY_TOOL_MAKE_NOACCESS(reinterpret_cast<uint8_t*>(result) + num_bytes,
                    usable_size - (num_bytes + kMemoryToolRedZoneBytes));

  return result;
}

inline size_t GetObjSizeNoThreadSafety(mirror::Object* obj) NO_THREAD_SAFETY_ANALYSIS {
  return obj->SizeOf<kVerifyNone>();
}

}  // namespace memory_tool_details

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
mirror::Object*
MemoryToolMallocSpace<S,
                    kMemoryToolRedZoneBytes,
                    kAdjustForRedzoneInAllocSize,
                    kUseObjSizeForUsable>::AllocWithGrowth(
    Thread* self, size_t num_bytes, size_t* bytes_allocated_out, size_t* usable_size_out,
    size_t* bytes_tl_bulk_allocated_out) {
  size_t bytes_allocated;
  size_t usable_size;
  size_t bytes_tl_bulk_allocated;
  void* obj_with_rdz = S::AllocWithGrowth(self, num_bytes + 2 * kMemoryToolRedZoneBytes,
                                          &bytes_allocated, &usable_size,
                                          &bytes_tl_bulk_allocated);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }

  return memory_tool_details::AdjustForValgrind<kMemoryToolRedZoneBytes, kUseObjSizeForUsable>(
      obj_with_rdz, num_bytes,
      bytes_allocated, usable_size,
      bytes_tl_bulk_allocated,
      bytes_allocated_out,
      usable_size_out,
      bytes_tl_bulk_allocated_out);
}

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
mirror::Object* MemoryToolMallocSpace<S,
                                    kMemoryToolRedZoneBytes,
                                    kAdjustForRedzoneInAllocSize,
                                    kUseObjSizeForUsable>::Alloc(
    Thread* self, size_t num_bytes, size_t* bytes_allocated_out, size_t* usable_size_out,
    size_t* bytes_tl_bulk_allocated_out) {
  size_t bytes_allocated;
  size_t usable_size;
  size_t bytes_tl_bulk_allocated;
  void* obj_with_rdz = S::Alloc(self, num_bytes + 2 * kMemoryToolRedZoneBytes,
                                &bytes_allocated, &usable_size, &bytes_tl_bulk_allocated);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }

  return memory_tool_details::AdjustForValgrind<kMemoryToolRedZoneBytes,
                                             kUseObjSizeForUsable>(obj_with_rdz, num_bytes,
                                                                   bytes_allocated, usable_size,
                                                                   bytes_tl_bulk_allocated,
                                                                   bytes_allocated_out,
                                                                   usable_size_out,
                                                                   bytes_tl_bulk_allocated_out);
}

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
mirror::Object* MemoryToolMallocSpace<S,
                                    kMemoryToolRedZoneBytes,
                                    kAdjustForRedzoneInAllocSize,
                                    kUseObjSizeForUsable>::AllocThreadUnsafe(
    Thread* self, size_t num_bytes, size_t* bytes_allocated_out, size_t* usable_size_out,
    size_t* bytes_tl_bulk_allocated_out) {
  size_t bytes_allocated;
  size_t usable_size;
  size_t bytes_tl_bulk_allocated;
  void* obj_with_rdz = S::AllocThreadUnsafe(self, num_bytes + 2 * kMemoryToolRedZoneBytes,
                                            &bytes_allocated, &usable_size,
                                            &bytes_tl_bulk_allocated);
  if (obj_with_rdz == nullptr) {
    return nullptr;
  }

  return memory_tool_details::AdjustForValgrind<kMemoryToolRedZoneBytes, kUseObjSizeForUsable>(
      obj_with_rdz, num_bytes,
      bytes_allocated, usable_size,
      bytes_tl_bulk_allocated,
      bytes_allocated_out,
      usable_size_out,
      bytes_tl_bulk_allocated_out);
}

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t MemoryToolMallocSpace<S,
                           kMemoryToolRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::AllocationSize(
    mirror::Object* obj, size_t* usable_size) {
  size_t result = S::AllocationSize(reinterpret_cast<mirror::Object*>(
      reinterpret_cast<uint8_t*>(obj) - (kAdjustForRedzoneInAllocSize ? kMemoryToolRedZoneBytes : 0)),
      usable_size);
  if (usable_size != nullptr) {
    if (kUseObjSizeForUsable) {
      *usable_size = memory_tool_details::GetObjSizeNoThreadSafety(obj);
    } else {
      *usable_size = *usable_size - 2 * kMemoryToolRedZoneBytes;
    }
  }
  return result;
}

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t MemoryToolMallocSpace<S,
                           kMemoryToolRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::Free(
    Thread* self, mirror::Object* ptr) {
  void* obj_after_rdz = reinterpret_cast<void*>(ptr);
  uint8_t* obj_with_rdz = reinterpret_cast<uint8_t*>(obj_after_rdz) - kMemoryToolRedZoneBytes;

  // Make redzones undefined.
  size_t usable_size;
  size_t allocation_size = AllocationSize(ptr, &usable_size);

  // Unprotect the allocation.
  // Use the obj-size-for-usable flag to determine whether usable_size is the more important one,
  // e.g., whether there's data in the allocation_size (and usable_size can't be trusted).
  if (kUseObjSizeForUsable) {
    MEMORY_TOOL_MAKE_UNDEFINED(obj_with_rdz, allocation_size);
  } else {
    MEMORY_TOOL_MAKE_UNDEFINED(obj_with_rdz, usable_size + 2 * kMemoryToolRedZoneBytes);
  }

  return S::Free(self, reinterpret_cast<mirror::Object*>(obj_with_rdz));
}

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t MemoryToolMallocSpace<S,
                           kMemoryToolRedZoneBytes,
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
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
template <typename... Params>
MemoryToolMallocSpace<S,
                    kMemoryToolRedZoneBytes,
                    kAdjustForRedzoneInAllocSize,
                    kUseObjSizeForUsable>::MemoryToolMallocSpace(
    MemMap* mem_map, size_t initial_size, Params... params) : S(mem_map, initial_size, params...) {
  // Don't want to change the valgrind states of the mem map here as the allocator is already
  // initialized at this point and that may interfere with what the allocator does internally. Note
  // that the tail beyond the initial size is mprotected.
}

template <typename S,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
size_t MemoryToolMallocSpace<S,
                           kMemoryToolRedZoneBytes,
                           kAdjustForRedzoneInAllocSize,
                           kUseObjSizeForUsable>::MaxBytesBulkAllocatedFor(size_t num_bytes) {
  return S::MaxBytesBulkAllocatedFor(num_bytes + 2 * kMemoryToolRedZoneBytes);
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_MEMORY_TOOL_MALLOC_SPACE_INL_H_
