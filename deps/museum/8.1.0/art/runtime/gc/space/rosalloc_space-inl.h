/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_SPACE_ROSALLOC_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_ROSALLOC_SPACE_INL_H_

#include "rosalloc_space.h"

#include "base/memory_tool.h"
#include "gc/allocator/rosalloc-inl.h"
#include "gc/space/memory_tool_settings.h"
#include "thread.h"

namespace art {
namespace gc {
namespace space {

template<bool kThreadSafe>
inline mirror::Object* RosAllocSpace::AllocCommon(Thread* self, size_t num_bytes,
                                                  size_t* bytes_allocated, size_t* usable_size,
                                                  size_t* bytes_tl_bulk_allocated) {
  size_t rosalloc_bytes_allocated = 0;
  size_t rosalloc_usable_size = 0;
  size_t rosalloc_bytes_tl_bulk_allocated = 0;
  if (!kThreadSafe) {
    Locks::mutator_lock_->AssertExclusiveHeld(self);
  }
  mirror::Object* result = reinterpret_cast<mirror::Object*>(
      rosalloc_->Alloc<kThreadSafe>(self, num_bytes, &rosalloc_bytes_allocated,
                                    &rosalloc_usable_size,
                                    &rosalloc_bytes_tl_bulk_allocated));
  if (LIKELY(result != nullptr)) {
    if (kDebugSpaces) {
      CHECK(Contains(result)) << "Allocation (" << reinterpret_cast<void*>(result)
            << ") not in bounds of allocation space " << *this;
    }
    DCHECK(bytes_allocated != nullptr);
    *bytes_allocated = rosalloc_bytes_allocated;
    DCHECK_EQ(rosalloc_usable_size, rosalloc_->UsableSize(result));
    if (usable_size != nullptr) {
      *usable_size = rosalloc_usable_size;
    }
    DCHECK(bytes_tl_bulk_allocated != nullptr);
    *bytes_tl_bulk_allocated = rosalloc_bytes_tl_bulk_allocated;
  }
  return result;
}

inline bool RosAllocSpace::CanAllocThreadLocal(Thread* self, size_t num_bytes) {
  return rosalloc_->CanAllocFromThreadLocalRun(self, num_bytes);
}

inline mirror::Object* RosAllocSpace::AllocThreadLocal(Thread* self, size_t num_bytes,
                                                       size_t* bytes_allocated) {
  DCHECK(bytes_allocated != nullptr);
  return reinterpret_cast<mirror::Object*>(
      rosalloc_->AllocFromThreadLocalRun(self, num_bytes, bytes_allocated));
}

inline size_t RosAllocSpace::MaxBytesBulkAllocatedForNonvirtual(size_t num_bytes) {
  return rosalloc_->MaxBytesBulkAllocatedFor(num_bytes);
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_ROSALLOC_SPACE_INL_H_
