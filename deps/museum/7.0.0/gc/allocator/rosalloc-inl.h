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

#ifndef ART_RUNTIME_GC_ALLOCATOR_ROSALLOC_INL_H_
#define ART_RUNTIME_GC_ALLOCATOR_ROSALLOC_INL_H_

#include "rosalloc.h"

namespace art {
namespace gc {
namespace allocator {

inline ALWAYS_INLINE bool RosAlloc::ShouldCheckZeroMemory() {
  return kCheckZeroMemory && !is_running_on_memory_tool_;
}

template<bool kThreadSafe>
inline ALWAYS_INLINE void* RosAlloc::Alloc(Thread* self, size_t size, size_t* bytes_allocated,
                                           size_t* usable_size,
                                           size_t* bytes_tl_bulk_allocated) {
  if (UNLIKELY(size > kLargeSizeThreshold)) {
    return AllocLargeObject(self, size, bytes_allocated, usable_size,
                            bytes_tl_bulk_allocated);
  }
  void* m;
  if (kThreadSafe) {
    m = AllocFromRun(self, size, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
  } else {
    m = AllocFromRunThreadUnsafe(self, size, bytes_allocated, usable_size,
                                 bytes_tl_bulk_allocated);
  }
  // Check if the returned memory is really all zero.
  if (ShouldCheckZeroMemory() && m != nullptr) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(m);
    for (size_t i = 0; i < size; ++i) {
      DCHECK_EQ(bytes[i], 0);
    }
  }
  return m;
}

inline bool RosAlloc::Run::IsFull() {
  return free_list_.Size() == 0;
}

inline bool RosAlloc::CanAllocFromThreadLocalRun(Thread* self, size_t size) {
  if (UNLIKELY(!IsSizeForThreadLocal(size))) {
    return false;
  }
  size_t bracket_size;
  size_t idx = SizeToIndexAndBracketSize(size, &bracket_size);
  DCHECK_LT(idx, kNumThreadLocalSizeBrackets);
  Run* thread_local_run = reinterpret_cast<Run*>(self->GetRosAllocRun(idx));
  if (kIsDebugBuild) {
    // Need the lock to prevent race conditions.
    MutexLock mu(self, *size_bracket_locks_[idx]);
    CHECK(non_full_runs_[idx].find(thread_local_run) == non_full_runs_[idx].end());
    CHECK(full_runs_[idx].find(thread_local_run) == full_runs_[idx].end());
  }
  DCHECK(thread_local_run != nullptr);
  DCHECK(thread_local_run->IsThreadLocal() || thread_local_run == dedicated_full_run_);
  return !thread_local_run->IsFull();
}

inline void* RosAlloc::AllocFromThreadLocalRun(Thread* self, size_t size,
                                               size_t* bytes_allocated) {
  DCHECK(bytes_allocated != nullptr);
  if (UNLIKELY(!IsSizeForThreadLocal(size))) {
    return nullptr;
  }
  size_t bracket_size;
  size_t idx = SizeToIndexAndBracketSize(size, &bracket_size);
  Run* thread_local_run = reinterpret_cast<Run*>(self->GetRosAllocRun(idx));
  if (kIsDebugBuild) {
    // Need the lock to prevent race conditions.
    MutexLock mu(self, *size_bracket_locks_[idx]);
    CHECK(non_full_runs_[idx].find(thread_local_run) == non_full_runs_[idx].end());
    CHECK(full_runs_[idx].find(thread_local_run) == full_runs_[idx].end());
  }
  DCHECK(thread_local_run != nullptr);
  DCHECK(thread_local_run->IsThreadLocal() || thread_local_run == dedicated_full_run_);
  void* slot_addr = thread_local_run->AllocSlot();
  if (LIKELY(slot_addr != nullptr)) {
    *bytes_allocated = bracket_size;
  }
  return slot_addr;
}

inline size_t RosAlloc::MaxBytesBulkAllocatedFor(size_t size) {
  if (UNLIKELY(!IsSizeForThreadLocal(size))) {
    return size;
  }
  size_t bracket_size;
  size_t idx = SizeToIndexAndBracketSize(size, &bracket_size);
  return numOfSlots[idx] * bracket_size;
}

inline void* RosAlloc::Run::AllocSlot() {
  Slot* slot = free_list_.Remove();
  if (kTraceRosAlloc && slot != nullptr) {
    const uint8_t idx = size_bracket_idx_;
    LOG(INFO) << "RosAlloc::Run::AllocSlot() : " << slot
              << ", bracket_size=" << std::dec << bracketSizes[idx]
              << ", slot_idx=" << SlotIndex(slot);
  }
  return slot;
}

}  // namespace allocator
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ALLOCATOR_ROSALLOC_INL_H_
