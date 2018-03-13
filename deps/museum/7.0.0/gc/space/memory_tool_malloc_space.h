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

#ifndef ART_RUNTIME_GC_SPACE_MEMORY_TOOL_MALLOC_SPACE_H_
#define ART_RUNTIME_GC_SPACE_MEMORY_TOOL_MALLOC_SPACE_H_

#include "malloc_space.h"

namespace art {
namespace gc {
namespace space {

// A specialization of DlMallocSpace/RosAllocSpace that places memory tool red
// zones around allocations.
template <typename BaseMallocSpaceType,
          size_t kMemoryToolRedZoneBytes,
          bool kAdjustForRedzoneInAllocSize,
          bool kUseObjSizeForUsable>
class MemoryToolMallocSpace FINAL : public BaseMallocSpaceType {
 public:
  mirror::Object* AllocWithGrowth(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                                  size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      OVERRIDE;
  mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                        size_t* usable_size, size_t* bytes_tl_bulk_allocated) OVERRIDE;
  mirror::Object* AllocThreadUnsafe(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                                    size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      OVERRIDE REQUIRES(Locks::mutator_lock_);

  size_t AllocationSize(mirror::Object* obj, size_t* usable_size) OVERRIDE;

  size_t Free(Thread* self, mirror::Object* ptr) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_);

  size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_);

  void RegisterRecentFree(mirror::Object* ptr ATTRIBUTE_UNUSED) OVERRIDE {}

  size_t MaxBytesBulkAllocatedFor(size_t num_bytes) OVERRIDE;

  template <typename... Params>
  MemoryToolMallocSpace(MemMap* mem_map, size_t initial_size, Params... params);
  virtual ~MemoryToolMallocSpace() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryToolMallocSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_MEMORY_TOOL_MALLOC_SPACE_H_
