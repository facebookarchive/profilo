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

#ifndef ART_RUNTIME_GC_ALLOCATOR_ROSALLOC_H_
#define ART_RUNTIME_GC_ALLOCATOR_ROSALLOC_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/allocator.h"
#include "base/bit_utils.h"
#include "base/mutex.h"
#include "base/logging.h"
#include "globals.h"
#include "thread.h"

namespace art {

class MemMap;

namespace gc {
namespace allocator {

// A runs-of-slots memory allocator.
class RosAlloc {
 private:
  // Represents a run of free pages.
  class FreePageRun {
   public:
    uint8_t magic_num_;  // The magic number used for debugging only.

    bool IsFree() const {
      return !kIsDebugBuild || magic_num_ == kMagicNumFree;
    }
    size_t ByteSize(RosAlloc* rosalloc) const EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      const uint8_t* fpr_base = reinterpret_cast<const uint8_t*>(this);
      size_t pm_idx = rosalloc->ToPageMapIndex(fpr_base);
      size_t byte_size = rosalloc->free_page_run_size_map_[pm_idx];
      DCHECK_GE(byte_size, static_cast<size_t>(0));
      DCHECK_ALIGNED(byte_size, kPageSize);
      return byte_size;
    }
    void SetByteSize(RosAlloc* rosalloc, size_t byte_size)
        EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      DCHECK_EQ(byte_size % kPageSize, static_cast<size_t>(0));
      uint8_t* fpr_base = reinterpret_cast<uint8_t*>(this);
      size_t pm_idx = rosalloc->ToPageMapIndex(fpr_base);
      rosalloc->free_page_run_size_map_[pm_idx] = byte_size;
    }
    void* Begin() {
      return reinterpret_cast<void*>(this);
    }
    void* End(RosAlloc* rosalloc) EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      uint8_t* fpr_base = reinterpret_cast<uint8_t*>(this);
      uint8_t* end = fpr_base + ByteSize(rosalloc);
      return end;
    }
    bool IsLargerThanPageReleaseThreshold(RosAlloc* rosalloc)
        EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      return ByteSize(rosalloc) >= rosalloc->page_release_size_threshold_;
    }
    bool IsAtEndOfSpace(RosAlloc* rosalloc)
        EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      return reinterpret_cast<uint8_t*>(this) + ByteSize(rosalloc) == rosalloc->base_ + rosalloc->footprint_;
    }
    bool ShouldReleasePages(RosAlloc* rosalloc) EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      switch (rosalloc->page_release_mode_) {
        case kPageReleaseModeNone:
          return false;
        case kPageReleaseModeEnd:
          return IsAtEndOfSpace(rosalloc);
        case kPageReleaseModeSize:
          return IsLargerThanPageReleaseThreshold(rosalloc);
        case kPageReleaseModeSizeAndEnd:
          return IsLargerThanPageReleaseThreshold(rosalloc) && IsAtEndOfSpace(rosalloc);
        case kPageReleaseModeAll:
          return true;
        default:
          LOG(FATAL) << "Unexpected page release mode ";
          return false;
      }
    }
    void ReleasePages(RosAlloc* rosalloc) EXCLUSIVE_LOCKS_REQUIRED(rosalloc->lock_) {
      uint8_t* start = reinterpret_cast<uint8_t*>(this);
      size_t byte_size = ByteSize(rosalloc);
      DCHECK_EQ(byte_size % kPageSize, static_cast<size_t>(0));
      if (ShouldReleasePages(rosalloc)) {
        rosalloc->ReleasePageRange(start, start + byte_size);
      }
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(FreePageRun);
  };

  // Represents a run of memory slots of the same size.
  //
  // A run's memory layout:
  //
  // +-------------------+
  // | magic_num         |
  // +-------------------+
  // | size_bracket_idx  |
  // +-------------------+
  // | is_thread_local   |
  // +-------------------+
  // | to_be_bulk_freed  |
  // +-------------------+
  // | top_bitmap_idx    |
  // +-------------------+
  // |                   |
  // | alloc bit map     |
  // |                   |
  // +-------------------+
  // |                   |
  // | bulk free bit map |
  // |                   |
  // +-------------------+
  // |                   |
  // | thread-local free |
  // | bit map           |
  // |                   |
  // +-------------------+
  // | padding due to    |
  // | alignment         |
  // +-------------------+
  // | slot 0            |
  // +-------------------+
  // | slot 1            |
  // +-------------------+
  // | slot 2            |
  // +-------------------+
  // ...
  // +-------------------+
  // | last slot         |
  // +-------------------+
  //
  class Run {
   public:
    uint8_t magic_num_;                 // The magic number used for debugging.
    uint8_t size_bracket_idx_;          // The index of the size bracket of this run.
    uint8_t is_thread_local_;           // True if this run is used as a thread-local run.
    uint8_t to_be_bulk_freed_;          // Used within BulkFree() to flag a run that's involved with a bulk free.
    uint32_t first_search_vec_idx_;  // The index of the first bitmap vector which may contain an available slot.
    uint32_t alloc_bit_map_[0];      // The bit map that allocates if each slot is in use.

    // bulk_free_bit_map_[] : The bit map that is used for GC to
    // temporarily mark the slots to free without using a lock. After
    // all the slots to be freed in a run are marked, all those slots
    // get freed in bulk with one locking per run, as opposed to one
    // locking per slot to minimize the lock contention. This is used
    // within BulkFree().

    // thread_local_free_bit_map_[] : The bit map that is used for GC
    // to temporarily mark the slots to free in a thread-local run
    // without using a lock (without synchronizing the thread that
    // owns the thread-local run.) When the thread-local run becomes
    // full, the thread will check this bit map and update the
    // allocation bit map of the run (that is, the slots get freed.)

    // Returns the byte size of the header except for the bit maps.
    static size_t fixed_header_size() {
      Run temp;
      size_t size = reinterpret_cast<uint8_t*>(&temp.alloc_bit_map_) - reinterpret_cast<uint8_t*>(&temp);
      DCHECK_EQ(size, static_cast<size_t>(8));
      return size;
    }
    // Returns the base address of the free bit map.
    uint32_t* BulkFreeBitMap() {
      return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + bulkFreeBitMapOffsets[size_bracket_idx_]);
    }
    // Returns the base address of the thread local free bit map.
    uint32_t* ThreadLocalFreeBitMap() {
      return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + threadLocalFreeBitMapOffsets[size_bracket_idx_]);
    }
    void* End() {
      return reinterpret_cast<uint8_t*>(this) + kPageSize * numOfPages[size_bracket_idx_];
    }
    // Returns the number of bitmap words per run.
    size_t NumberOfBitmapVectors() const {
      return RoundUp(numOfSlots[size_bracket_idx_], 32) / 32;
    }
    void SetIsThreadLocal(bool is_thread_local) {
      is_thread_local_  = is_thread_local ? 1 : 0;
    }
    bool IsThreadLocal() const {
      return is_thread_local_ != 0;
    }
    // Frees slots in the allocation bit map with regard to the
    // thread-local free bit map. Used when a thread-local run becomes
    // full.
    bool MergeThreadLocalFreeBitMapToAllocBitMap(bool* is_all_free_after_out);
    // Frees slots in the allocation bit map with regard to the bulk
    // free bit map. Used in a bulk free.
    void MergeBulkFreeBitMapIntoAllocBitMap();
    // Unions the slots to be freed in the free bit map into the
    // thread-local free bit map. In a bulk free, as a two-step
    // process, GC will first record all the slots to free in a run in
    // the free bit map where it can write without a lock, and later
    // acquire a lock once per run to union the bits of the free bit
    // map to the thread-local free bit map.
    void UnionBulkFreeBitMapToThreadLocalFreeBitMap();
    // Allocates a slot in a run.
    void* AllocSlot();
    // Frees a slot in a run. This is used in a non-bulk free.
    void FreeSlot(void* ptr);
    // Marks the slots to free in the bulk free bit map. Returns the bracket size.
    size_t MarkBulkFreeBitMap(void* ptr);
    // Marks the slots to free in the thread-local free bit map.
    void MarkThreadLocalFreeBitMap(void* ptr);
    // Last word mask, all of the bits in the last word which aren't valid slots are set to
    // optimize allocation path.
    static uint32_t GetBitmapLastVectorMask(size_t num_slots, size_t num_vec);
    // Returns true if all the slots in the run are not in use.
    bool IsAllFree();
    // Returns the number of free slots.
    size_t NumberOfFreeSlots();
    // Returns true if all the slots in the run are in use.
    ALWAYS_INLINE bool IsFull();
    // Returns true if the bulk free bit map is clean.
    bool IsBulkFreeBitmapClean();
    // Returns true if the thread local free bit map is clean.
    bool IsThreadLocalFreeBitmapClean();
    // Set the alloc_bit_map_ bits for slots that are past the end of the run.
    void SetAllocBitMapBitsForInvalidSlots();
    // Zero the run's data.
    void ZeroData();
    // Zero the run's header.
    void ZeroHeader();
    // Fill the alloc bitmap with 1s.
    void FillAllocBitMap();
    // Iterate over all the slots and apply the given function.
    void InspectAllSlots(void (*handler)(void* start, void* end, size_t used_bytes, void* callback_arg), void* arg);
    // Dump the run metadata for debugging.
    std::string Dump();
    // Verify for debugging.
    void Verify(Thread* self, RosAlloc* rosalloc, bool running_on_valgrind)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_);

   private:
    // The common part of MarkFreeBitMap() and MarkThreadLocalFreeBitMap(). Returns the bracket
    // size.
    size_t MarkFreeBitMapShared(void* ptr, uint32_t* free_bit_map_base, const char* caller_name);
    // Turns the bit map into a string for debugging.
    static std::string BitMapToStr(uint32_t* bit_map_base, size_t num_vec);

    // TODO: DISALLOW_COPY_AND_ASSIGN(Run);
  };

  // The magic number for a run.
  static constexpr uint8_t kMagicNum = 42;
  // The magic number for free pages.
  static constexpr uint8_t kMagicNumFree = 43;
  // The number of size brackets. Sync this with the length of Thread::rosalloc_runs_.
  static constexpr size_t kNumOfSizeBrackets = kNumRosAllocThreadLocalSizeBrackets;
  // The number of smaller size brackets that are 16 bytes apart.
  static constexpr size_t kNumOfQuantumSizeBrackets = 32;
  // The sizes (the slot sizes, in bytes) of the size brackets.
  static size_t bracketSizes[kNumOfSizeBrackets];
  // The numbers of pages that are used for runs for each size bracket.
  static size_t numOfPages[kNumOfSizeBrackets];
  // The numbers of slots of the runs for each size bracket.
  static size_t numOfSlots[kNumOfSizeBrackets];
  // The header sizes in bytes of the runs for each size bracket.
  static size_t headerSizes[kNumOfSizeBrackets];
  // The byte offsets of the bulk free bit maps of the runs for each size bracket.
  static size_t bulkFreeBitMapOffsets[kNumOfSizeBrackets];
  // The byte offsets of the thread-local free bit maps of the runs for each size bracket.
  static size_t threadLocalFreeBitMapOffsets[kNumOfSizeBrackets];

  // Initialize the run specs (the above arrays).
  static void Initialize();
  static bool initialized_;

  // Returns the byte size of the bracket size from the index.
  static size_t IndexToBracketSize(size_t idx) {
    DCHECK_LT(idx, kNumOfSizeBrackets);
    return bracketSizes[idx];
  }
  // Returns the index of the size bracket from the bracket size.
  static size_t BracketSizeToIndex(size_t size) {
    DCHECK(16 <= size && ((size < 1 * KB && size % 16 == 0) || size == 1 * KB || size == 2 * KB));
    size_t idx;
    if (UNLIKELY(size == 1 * KB)) {
      idx = kNumOfSizeBrackets - 2;
    } else if (UNLIKELY(size == 2 * KB)) {
      idx = kNumOfSizeBrackets - 1;
    } else {
      DCHECK(size < 1 * KB);
      DCHECK_EQ(size % 16, static_cast<size_t>(0));
      idx = size / 16 - 1;
    }
    DCHECK(bracketSizes[idx] == size);
    return idx;
  }
  // Returns true if the given allocation size is for a thread local allocation.
  static bool IsSizeForThreadLocal(size_t size) {
    DCHECK_GT(kNumThreadLocalSizeBrackets, 0U);
    size_t max_thread_local_bracket_idx = kNumThreadLocalSizeBrackets - 1;
    bool is_size_for_thread_local = size <= bracketSizes[max_thread_local_bracket_idx];
    DCHECK(size > kLargeSizeThreshold ||
           (is_size_for_thread_local == (SizeToIndex(size) < kNumThreadLocalSizeBrackets)));
    return is_size_for_thread_local;
  }
  // Rounds up the size up the nearest bracket size.
  static size_t RoundToBracketSize(size_t size) {
    DCHECK(size <= kLargeSizeThreshold);
    if (LIKELY(size <= 512)) {
      return RoundUp(size, 16);
    } else if (512 < size && size <= 1 * KB) {
      return 1 * KB;
    } else {
      DCHECK(1 * KB < size && size <= 2 * KB);
      return 2 * KB;
    }
  }
  // Returns the size bracket index from the byte size with rounding.
  static size_t SizeToIndex(size_t size) {
    DCHECK(size <= kLargeSizeThreshold);
    if (LIKELY(size <= 512)) {
      return RoundUp(size, 16) / 16 - 1;
    } else if (512 < size && size <= 1 * KB) {
      return kNumOfSizeBrackets - 2;
    } else {
      DCHECK(1 * KB < size && size <= 2 * KB);
      return kNumOfSizeBrackets - 1;
    }
  }
  // A combination of SizeToIndex() and RoundToBracketSize().
  static size_t SizeToIndexAndBracketSize(size_t size, size_t* bracket_size_out) {
    DCHECK(size <= kLargeSizeThreshold);
    if (LIKELY(size <= 512)) {
      size_t bracket_size = RoundUp(size, 16);
      *bracket_size_out = bracket_size;
      size_t idx = bracket_size / 16 - 1;
      DCHECK_EQ(bracket_size, IndexToBracketSize(idx));
      return idx;
    } else if (512 < size && size <= 1 * KB) {
      size_t bracket_size = 1024;
      *bracket_size_out = bracket_size;
      size_t idx = kNumOfSizeBrackets - 2;
      DCHECK_EQ(bracket_size, IndexToBracketSize(idx));
      return idx;
    } else {
      DCHECK(1 * KB < size && size <= 2 * KB);
      size_t bracket_size = 2048;
      *bracket_size_out = bracket_size;
      size_t idx = kNumOfSizeBrackets - 1;
      DCHECK_EQ(bracket_size, IndexToBracketSize(idx));
      return idx;
    }
  }
  // Returns the page map index from an address. Requires that the
  // address is page size aligned.
  size_t ToPageMapIndex(const void* addr) const {
    DCHECK_LE(base_, addr);
    DCHECK_LT(addr, base_ + capacity_);
    size_t byte_offset = reinterpret_cast<const uint8_t*>(addr) - base_;
    DCHECK_EQ(byte_offset % static_cast<size_t>(kPageSize), static_cast<size_t>(0));
    return byte_offset / kPageSize;
  }
  // Returns the page map index from an address with rounding.
  size_t RoundDownToPageMapIndex(const void* addr) const {
    DCHECK(base_ <= addr && addr < reinterpret_cast<uint8_t*>(base_) + capacity_);
    return (reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(base_)) / kPageSize;
  }

  // A memory allocation request larger than this size is treated as a large object and allocated
  // at a page-granularity.
  static const size_t kLargeSizeThreshold = 2048;

  // If true, check that the returned memory is actually zero.
  static constexpr bool kCheckZeroMemory = kIsDebugBuild;
  // Valgrind protects memory, so do not check memory when running under valgrind. In a normal
  // build with kCheckZeroMemory the whole test should be optimized away.
  // TODO: Unprotect before checks.
  ALWAYS_INLINE bool ShouldCheckZeroMemory();

  // If true, log verbose details of operations.
  static constexpr bool kTraceRosAlloc = false;

  struct hash_run {
    size_t operator()(const RosAlloc::Run* r) const {
      return reinterpret_cast<size_t>(r);
    }
  };

  struct eq_run {
    bool operator()(const RosAlloc::Run* r1, const RosAlloc::Run* r2) const {
      return r1 == r2;
    }
  };

 public:
  // Different page release modes.
  enum PageReleaseMode {
    kPageReleaseModeNone,         // Release no empty pages.
    kPageReleaseModeEnd,          // Release empty pages at the end of the space.
    kPageReleaseModeSize,         // Release empty pages that are larger than the threshold.
    kPageReleaseModeSizeAndEnd,   // Release empty pages that are larger than the threshold or
                                  // at the end of the space.
    kPageReleaseModeAll,          // Release all empty pages.
  };

  // The default value for page_release_size_threshold_.
  static constexpr size_t kDefaultPageReleaseSizeThreshold = 4 * MB;

  // We use thread-local runs for the size Brackets whose indexes
  // are less than this index. We use shared (current) runs for the rest.
  static const size_t kNumThreadLocalSizeBrackets = 8;

 private:
  // The base address of the memory region that's managed by this allocator.
  uint8_t* base_;

  // The footprint in bytes of the currently allocated portion of the
  // memory region.
  size_t footprint_;

  // The maximum footprint. The address, base_ + capacity_, indicates
  // the end of the memory region that's currently managed by this allocator.
  size_t capacity_;

  // The maximum capacity. The address, base_ + max_capacity_, indicates
  // the end of the memory region that's ever managed by this allocator.
  size_t max_capacity_;

  // The run sets that hold the runs whose slots are not all
  // full. non_full_runs_[i] is guarded by size_bracket_locks_[i].
  AllocationTrackingSet<Run*, kAllocatorTagRosAlloc> non_full_runs_[kNumOfSizeBrackets];
  // The run sets that hold the runs whose slots are all full. This is
  // debug only. full_runs_[i] is guarded by size_bracket_locks_[i].
  std::unordered_set<Run*, hash_run, eq_run, TrackingAllocator<Run*, kAllocatorTagRosAlloc>>
      full_runs_[kNumOfSizeBrackets];
  // The set of free pages.
  AllocationTrackingSet<FreePageRun*, kAllocatorTagRosAlloc> free_page_runs_ GUARDED_BY(lock_);
  // The dedicated full run, it is always full and shared by all threads when revoking happens.
  // This is an optimization since enables us to avoid a null check for revoked runs.
  static Run* dedicated_full_run_;
  // Using size_t to ensure that it is at least word aligned.
  static size_t dedicated_full_run_storage_[];
  // The current runs where the allocations are first attempted for
  // the size brackes that do not use thread-local
  // runs. current_runs_[i] is guarded by size_bracket_locks_[i].
  Run* current_runs_[kNumOfSizeBrackets];
  // The mutexes, one per size bracket.
  Mutex* size_bracket_locks_[kNumOfSizeBrackets];
  // Bracket lock names (since locks only have char* names).
  std::string size_bracket_lock_names_[kNumOfSizeBrackets];
  // The types of page map entries.
  enum PageMapKind {
    kPageMapReleased = 0,     // Zero and released back to the OS.
    kPageMapEmpty,            // Zero but probably dirty.
    kPageMapRun,              // The beginning of a run.
    kPageMapRunPart,          // The non-beginning part of a run.
    kPageMapLargeObject,      // The beginning of a large object.
    kPageMapLargeObjectPart,  // The non-beginning part of a large object.
  };
  // The table that indicates what pages are currently used for.
  volatile uint8_t* page_map_;  // No GUARDED_BY(lock_) for kReadPageMapEntryWithoutLockInBulkFree.
  size_t page_map_size_;
  size_t max_page_map_size_;
  std::unique_ptr<MemMap> page_map_mem_map_;

  // The table that indicates the size of free page runs. These sizes
  // are stored here to avoid storing in the free page header and
  // release backing pages.
  std::vector<size_t, TrackingAllocator<size_t, kAllocatorTagRosAlloc>> free_page_run_size_map_
      GUARDED_BY(lock_);
  // The global lock. Used to guard the page map, the free page set,
  // and the footprint.
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  // The reader-writer lock to allow one bulk free at a time while
  // allowing multiple individual frees at the same time. Also, this
  // is used to avoid race conditions between BulkFree() and
  // RevokeThreadLocalRuns() on the bulk free bitmaps.
  ReaderWriterMutex bulk_free_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // The page release mode.
  const PageReleaseMode page_release_mode_;
  // Under kPageReleaseModeSize(AndEnd), if the free page run size is
  // greater than or equal to this value, release pages.
  const size_t page_release_size_threshold_;

  // Whether this allocator is running under Valgrind.
  bool running_on_valgrind_;

  // The base address of the memory region that's managed by this allocator.
  uint8_t* Begin() { return base_; }
  // The end address of the memory region that's managed by this allocator.
  uint8_t* End() { return base_ + capacity_; }

  // Page-granularity alloc/free
  void* AllocPages(Thread* self, size_t num_pages, uint8_t page_map_type)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Returns how many bytes were freed.
  size_t FreePages(Thread* self, void* ptr, bool already_zero) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Allocate/free a run slot.
  void* AllocFromRun(Thread* self, size_t size, size_t* bytes_allocated, size_t* usable_size,
                     size_t* bytes_tl_bulk_allocated)
      LOCKS_EXCLUDED(lock_);
  // Allocate/free a run slot without acquiring locks.
  // TODO: EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
  void* AllocFromRunThreadUnsafe(Thread* self, size_t size, size_t* bytes_allocated,
                                 size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      LOCKS_EXCLUDED(lock_);
  void* AllocFromCurrentRunUnlocked(Thread* self, size_t idx);

  // Returns the bracket size.
  size_t FreeFromRun(Thread* self, void* ptr, Run* run)
      LOCKS_EXCLUDED(lock_);

  // Used to allocate a new thread local run for a size bracket.
  Run* AllocRun(Thread* self, size_t idx) LOCKS_EXCLUDED(lock_);

  // Used to acquire a new/reused run for a size bracket. Used when a
  // thread-local or current run gets full.
  Run* RefillRun(Thread* self, size_t idx) LOCKS_EXCLUDED(lock_);

  // The internal of non-bulk Free().
  size_t FreeInternal(Thread* self, void* ptr) LOCKS_EXCLUDED(lock_);

  // Allocates large objects.
  void* AllocLargeObject(Thread* self, size_t size, size_t* bytes_allocated,
                         size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      LOCKS_EXCLUDED(lock_);

  // Revoke a run by adding it to non_full_runs_ or freeing the pages.
  void RevokeRun(Thread* self, size_t idx, Run* run);

  // Revoke the current runs which share an index with the thread local runs.
  void RevokeThreadUnsafeCurrentRuns();

  // Release a range of pages.
  size_t ReleasePageRange(uint8_t* start, uint8_t* end) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Dumps the page map for debugging.
  std::string DumpPageMap() EXCLUSIVE_LOCKS_REQUIRED(lock_);

 public:
  RosAlloc(void* base, size_t capacity, size_t max_capacity,
           PageReleaseMode page_release_mode,
           bool running_on_valgrind,
           size_t page_release_size_threshold = kDefaultPageReleaseSizeThreshold);
  ~RosAlloc();

  // If kThreadUnsafe is true then the allocator may avoid acquiring some locks as an optimization.
  // If used, this may cause race conditions if multiple threads are allocating at the same time.
  template<bool kThreadSafe = true>
  void* Alloc(Thread* self, size_t size, size_t* bytes_allocated, size_t* usable_size,
              size_t* bytes_tl_bulk_allocated)
      LOCKS_EXCLUDED(lock_);
  size_t Free(Thread* self, void* ptr)
      LOCKS_EXCLUDED(bulk_free_lock_);
  size_t BulkFree(Thread* self, void** ptrs, size_t num_ptrs)
      LOCKS_EXCLUDED(bulk_free_lock_);

  // Returns true if the given allocation request can be allocated in
  // an existing thread local run without allocating a new run.
  ALWAYS_INLINE bool CanAllocFromThreadLocalRun(Thread* self, size_t size);
  // Allocate the given allocation request in an existing thread local
  // run without allocating a new run.
  ALWAYS_INLINE void* AllocFromThreadLocalRun(Thread* self, size_t size, size_t* bytes_allocated);

  // Returns the maximum bytes that could be allocated for the given
  // size in bulk, that is the maximum value for the
  // bytes_allocated_bulk out param returned by RosAlloc::Alloc().
  ALWAYS_INLINE size_t MaxBytesBulkAllocatedFor(size_t size);

  // Returns the size of the allocated slot for a given allocated memory chunk.
  size_t UsableSize(const void* ptr);
  // Returns the size of the allocated slot for a given size.
  size_t UsableSize(size_t bytes) {
    if (UNLIKELY(bytes > kLargeSizeThreshold)) {
      return RoundUp(bytes, kPageSize);
    } else {
      return RoundToBracketSize(bytes);
    }
  }
  // Try to reduce the current footprint by releasing the free page
  // run at the end of the memory region, if any.
  bool Trim();
  // Iterates over all the memory slots and apply the given function.
  void InspectAll(void (*handler)(void* start, void* end, size_t used_bytes, void* callback_arg),
                  void* arg)
      LOCKS_EXCLUDED(lock_);

  // Release empty pages.
  size_t ReleasePages() LOCKS_EXCLUDED(lock_);
  // Returns the current footprint.
  size_t Footprint() LOCKS_EXCLUDED(lock_);
  // Returns the current capacity, maximum footprint.
  size_t FootprintLimit() LOCKS_EXCLUDED(lock_);
  // Update the current capacity.
  void SetFootprintLimit(size_t bytes) LOCKS_EXCLUDED(lock_);

  // Releases the thread-local runs assigned to the given thread back to the common set of runs.
  // Returns the total bytes of free slots in the revoked thread local runs. This is to be
  // subtracted from Heap::num_bytes_allocated_ to cancel out the ahead-of-time counting.
  size_t RevokeThreadLocalRuns(Thread* thread);
  // Releases the thread-local runs assigned to all the threads back to the common set of runs.
  // Returns the total bytes of free slots in the revoked thread local runs. This is to be
  // subtracted from Heap::num_bytes_allocated_ to cancel out the ahead-of-time counting.
  size_t RevokeAllThreadLocalRuns() LOCKS_EXCLUDED(Locks::thread_list_lock_);
  // Assert the thread local runs of a thread are revoked.
  void AssertThreadLocalRunsAreRevoked(Thread* thread);
  // Assert all the thread local runs are revoked.
  void AssertAllThreadLocalRunsAreRevoked() LOCKS_EXCLUDED(Locks::thread_list_lock_);

  static Run* GetDedicatedFullRun() {
    return dedicated_full_run_;
  }
  bool IsFreePage(size_t idx) const {
    DCHECK_LT(idx, capacity_ / kPageSize);
    uint8_t pm_type = page_map_[idx];
    return pm_type == kPageMapReleased || pm_type == kPageMapEmpty;
  }

  // Callbacks for InspectAll that will count the number of bytes
  // allocated and objects allocated, respectively.
  static void BytesAllocatedCallback(void* start, void* end, size_t used_bytes, void* arg);
  static void ObjectsAllocatedCallback(void* start, void* end, size_t used_bytes, void* arg);

  bool DoesReleaseAllPages() const {
    return page_release_mode_ == kPageReleaseModeAll;
  }

  // Verify for debugging.
  void Verify() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes);

 private:
  friend std::ostream& operator<<(std::ostream& os, const RosAlloc::PageMapKind& rhs);

  DISALLOW_COPY_AND_ASSIGN(RosAlloc);
};
std::ostream& operator<<(std::ostream& os, const RosAlloc::PageMapKind& rhs);

// Callback from rosalloc when it needs to increase the footprint. Must be implemented somewhere
// else (currently rosalloc_space.cc).
void* ArtRosAllocMoreCore(allocator::RosAlloc* rosalloc, intptr_t increment);

}  // namespace allocator
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ALLOCATOR_ROSALLOC_H_
