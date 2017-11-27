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

#ifndef ART_RUNTIME_MONITOR_POOL_H_
#define ART_RUNTIME_MONITOR_POOL_H_

#include "monitor.h"

#include "base/allocator.h"
#ifdef __LP64__
#include <stdint.h>
#include "atomic.h"
#include "runtime.h"
#else
#include "base/stl_util.h"     // STLDeleteElements
#endif

namespace art {

// Abstraction to keep monitors small enough to fit in a lock word (32bits). On 32bit systems the
// monitor id loses the alignment bits of the Monitor*.
class MonitorPool {
 public:
  static MonitorPool* Create() {
#ifndef __LP64__
    return nullptr;
#else
    return new MonitorPool();
#endif
  }

  static Monitor* CreateMonitor(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code)
      SHARED_REQUIRES(Locks::mutator_lock_) {
#ifndef __LP64__
    Monitor* mon = new Monitor(self, owner, obj, hash_code);
    DCHECK_ALIGNED(mon, LockWord::kMonitorIdAlignment);
    return mon;
#else
    return GetMonitorPool()->CreateMonitorInPool(self, owner, obj, hash_code);
#endif
  }

  static void ReleaseMonitor(Thread* self, Monitor* monitor) {
#ifndef __LP64__
    UNUSED(self);
    delete monitor;
#else
    GetMonitorPool()->ReleaseMonitorToPool(self, monitor);
#endif
  }

  static void ReleaseMonitors(Thread* self, MonitorList::Monitors* monitors) {
#ifndef __LP64__
    UNUSED(self);
    STLDeleteElements(monitors);
#else
    GetMonitorPool()->ReleaseMonitorsToPool(self, monitors);
#endif
  }

  static Monitor* MonitorFromMonitorId(MonitorId mon_id) {
#ifndef __LP64__
    return reinterpret_cast<Monitor*>(mon_id << LockWord::kMonitorIdAlignmentShift);
#else
    return GetMonitorPool()->LookupMonitor(mon_id);
#endif
  }

  static MonitorId MonitorIdFromMonitor(Monitor* mon) {
#ifndef __LP64__
    return reinterpret_cast<MonitorId>(mon) >> LockWord::kMonitorIdAlignmentShift;
#else
    return mon->GetMonitorId();
#endif
  }

  static MonitorId ComputeMonitorId(Monitor* mon, Thread* self) {
#ifndef __LP64__
    UNUSED(self);
    return MonitorIdFromMonitor(mon);
#else
    return GetMonitorPool()->ComputeMonitorIdInPool(mon, self);
#endif
  }

  static MonitorPool* GetMonitorPool() {
#ifndef __LP64__
    return nullptr;
#else
    return Runtime::Current()->GetMonitorPool();
#endif
  }

  ~MonitorPool() {
#ifdef __LP64__
    FreeInternal();
#endif
  }

 private:
#ifdef __LP64__
  // When we create a monitor pool, threads have not been initialized, yet, so ignore thread-safety
  // analysis.
  MonitorPool() NO_THREAD_SAFETY_ANALYSIS;

  void AllocateChunk() REQUIRES(Locks::allocated_monitor_ids_lock_);

  // Release all chunks and metadata. This is done on shutdown, where threads have been destroyed,
  // so ignore thead-safety analysis.
  void FreeInternal() NO_THREAD_SAFETY_ANALYSIS;

  Monitor* CreateMonitorInPool(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void ReleaseMonitorToPool(Thread* self, Monitor* monitor);
  void ReleaseMonitorsToPool(Thread* self, MonitorList::Monitors* monitors);

  // Note: This is safe as we do not ever move chunks.  All needed entries in the monitor_chunks_
  // data structure are read-only once we get here.  Updates happen-before this call because
  // the lock word was stored with release semantics and we read it with acquire semantics to
  // retrieve the id.
  Monitor* LookupMonitor(MonitorId mon_id) {
    size_t offset = MonitorIdToOffset(mon_id);
    size_t index = offset / kChunkSize;
    size_t top_index = index / kMaxListSize;
    size_t list_index = index % kMaxListSize;
    size_t offset_in_chunk = offset % kChunkSize;
    uintptr_t base = monitor_chunks_[top_index][list_index];
    return reinterpret_cast<Monitor*>(base + offset_in_chunk);
  }

  static bool IsInChunk(uintptr_t base_addr, Monitor* mon) {
    uintptr_t mon_ptr = reinterpret_cast<uintptr_t>(mon);
    return base_addr <= mon_ptr && (mon_ptr - base_addr < kChunkSize);
  }

  MonitorId ComputeMonitorIdInPool(Monitor* mon, Thread* self) {
    MutexLock mu(self, *Locks::allocated_monitor_ids_lock_);
    for (size_t i = 0; i <= current_chunk_list_index_; ++i) {
      for (size_t j = 0; j < ChunkListCapacity(i); ++j) {
        if (j >= num_chunks_ && i == current_chunk_list_index_) {
          break;
        }
        uintptr_t chunk_addr = monitor_chunks_[i][j];
        if (IsInChunk(chunk_addr, mon)) {
          return OffsetToMonitorId(
              reinterpret_cast<uintptr_t>(mon) - chunk_addr
              + i * (kMaxListSize * kChunkSize) + j * kChunkSize);
        }
      }
    }
    LOG(FATAL) << "Did not find chunk that contains monitor.";
    return 0;
  }

  static constexpr size_t MonitorIdToOffset(MonitorId id) {
    return id << 3;
  }

  static constexpr MonitorId OffsetToMonitorId(size_t offset) {
    return static_cast<MonitorId>(offset >> 3);
  }

  static constexpr size_t ChunkListCapacity(size_t index) {
    return kInitialChunkStorage << index;
  }

  // TODO: There are assumptions in the code that monitor addresses are 8B aligned (>>3).
  static constexpr size_t kMonitorAlignment = 8;
  // Size of a monitor, rounded up to a multiple of alignment.
  static constexpr size_t kAlignedMonitorSize = (sizeof(Monitor) + kMonitorAlignment - 1) &
                                                -kMonitorAlignment;
  // As close to a page as we can get seems a good start.
  static constexpr size_t kChunkCapacity = kPageSize / kAlignedMonitorSize;
  // Chunk size that is referenced in the id. We can collapse this to the actually used storage
  // in a chunk, i.e., kChunkCapacity * kAlignedMonitorSize, but this will mean proper divisions.
  static constexpr size_t kChunkSize = kPageSize;
  static_assert(IsPowerOfTwo(kChunkSize), "kChunkSize must be power of 2");
  // The number of chunks of storage that can be referenced by the initial chunk list.
  // The total number of usable monitor chunks is typically 255 times this number, so it
  // should be large enough that we don't run out. We run out of address bits if it's > 512.
  // Currently we set it a bit smaller, to save half a page per process.  We make it tiny in
  // debug builds to catch growth errors. The only value we really expect to tune.
  static constexpr size_t kInitialChunkStorage = kIsDebugBuild ? 1U : 256U;
  static_assert(IsPowerOfTwo(kInitialChunkStorage), "kInitialChunkStorage must be power of 2");
  // The number of lists, each containing pointers to storage chunks.
  static constexpr size_t kMaxChunkLists = 8;  //  Dictated by 3 bit index. Don't increase above 8.
  static_assert(IsPowerOfTwo(kMaxChunkLists), "kMaxChunkLists must be power of 2");
  static constexpr size_t kMaxListSize = kInitialChunkStorage << (kMaxChunkLists - 1);
  // We lose 3 bits in monitor id due to 3 bit monitor_chunks_ index, and gain it back from
  // the 3 bit alignment constraint on monitors:
  static_assert(kMaxListSize * kChunkSize < (1 << LockWord::kMonitorIdSize),
      "Monitor id bits don't fit");
  static_assert(IsPowerOfTwo(kMaxListSize), "kMaxListSize must be power of 2");

  // Array of pointers to lists (again arrays) of pointers to chunks containing monitors.
  // Zeroth entry points to a list (array) of kInitialChunkStorage pointers to chunks.
  // Each subsequent list as twice as large as the preceding one.
  // Monitor Ids are interpreted as follows:
  //     Top 3 bits (of 28): index into monitor_chunks_.
  //     Next 16 bits: index into the chunk list, i.e. monitor_chunks_[i].
  //     Last 9 bits: offset within chunk, expressed as multiple of kMonitorAlignment.
  // If we set kInitialChunkStorage to 512, this would allow us to use roughly 128K chunks of
  // monitors, which is 0.5GB of monitors.  With this maximum setting, the largest chunk list
  // contains 64K entries, and we make full use of the available index space. With a
  // kInitialChunkStorage value of 256, this is proportionately reduced to 0.25GB of monitors.
  // Updates to monitor_chunks_ are guarded by allocated_monitor_ids_lock_ .
  // No field in this entire data structure is ever updated once a monitor id whose lookup
  // requires it has been made visible to another thread.  Thus readers never race with
  // updates, in spite of the fact that they acquire no locks.
  uintptr_t* monitor_chunks_[kMaxChunkLists];  //  uintptr_t is really a Monitor* .
  // Highest currently used index in monitor_chunks_ . Used for newly allocated chunks.
  size_t current_chunk_list_index_ GUARDED_BY(Locks::allocated_monitor_ids_lock_);
  // Number of chunk pointers stored in monitor_chunks_[current_chunk_list_index_] so far.
  size_t num_chunks_ GUARDED_BY(Locks::allocated_monitor_ids_lock_);
  // After the initial allocation, this is always equal to
  // ChunkListCapacity(current_chunk_list_index_).
  size_t current_chunk_list_capacity_ GUARDED_BY(Locks::allocated_monitor_ids_lock_);

  typedef TrackingAllocator<uint8_t, kAllocatorTagMonitorPool> Allocator;
  Allocator allocator_;

  // Start of free list of monitors.
  // Note: these point to the right memory regions, but do *not* denote initialized objects.
  Monitor* first_free_ GUARDED_BY(Locks::allocated_monitor_ids_lock_);
#endif
};

}  // namespace art

#endif  // ART_RUNTIME_MONITOR_POOL_H_
