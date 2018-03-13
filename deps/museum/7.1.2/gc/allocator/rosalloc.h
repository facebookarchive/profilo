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
    size_t ByteSize(RosAlloc* rosalloc) const REQUIRES(rosalloc->lock_) {
      const uint8_t* fpr_base = reinterpret_cast<const uint8_t*>(this);
      size_t pm_idx = rosalloc->ToPageMapIndex(fpr_base);
      size_t byte_size = rosalloc->free_page_run_size_map_[pm_idx];
      DCHECK_GE(byte_size, static_cast<size_t>(0));
      DCHECK_ALIGNED(byte_size, kPageSize);
      return byte_size;
    }
    void SetByteSize(RosAlloc* rosalloc, size_t byte_size)
        REQUIRES(rosalloc->lock_) {
      DCHECK_EQ(byte_size % kPageSize, static_cast<size_t>(0));
      uint8_t* fpr_base = reinterpret_cast<uint8_t*>(this);
      size_t pm_idx = rosalloc->ToPageMapIndex(fpr_base);
      rosalloc->free_page_run_size_map_[pm_idx] = byte_size;
    }
    void* Begin() {
      return reinterpret_cast<void*>(this);
    }
    void* End(RosAlloc* rosalloc) REQUIRES(rosalloc->lock_) {
      uint8_t* fpr_base = reinterpret_cast<uint8_t*>(this);
      uint8_t* end = fpr_base + ByteSize(rosalloc);
      return end;
    }
    bool IsLargerThanPageReleaseThreshold(RosAlloc* rosalloc)
        REQUIRES(rosalloc->lock_) {
      return ByteSize(rosalloc) >= rosalloc->page_release_size_threshold_;
    }
    bool IsAtEndOfSpace(RosAlloc* rosalloc)
        REQUIRES(rosalloc->lock_) {
      return reinterpret_cast<uint8_t*>(this) + ByteSize(rosalloc) == rosalloc->base_ + rosalloc->footprint_;
    }
    bool ShouldReleasePages(RosAlloc* rosalloc) REQUIRES(rosalloc->lock_) {
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
    void ReleasePages(RosAlloc* rosalloc) REQUIRES(rosalloc->lock_) {
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

  // The slot header.
  class Slot {
   public:
    Slot* Next() const {
      return next_;
    }
    void SetNext(Slot* next) {
      next_ = next;
    }
    // The slot right before this slot in terms of the address.
    Slot* Left(size_t bracket_size) {
      return reinterpret_cast<Slot*>(reinterpret_cast<uintptr_t>(this) - bracket_size);
    }
    void Clear() {
      next_ = nullptr;
    }

   private:
    Slot* next_;  // Next slot in the list.
    friend class RosAlloc;
  };

  // We use the tail (kUseTail == true) for the bulk or thread-local free lists to avoid the need to
  // traverse the list from the head to the tail when merging free lists.
  // We don't use the tail (kUseTail == false) for the free list to avoid the need to manage the
  // tail in the allocation fast path for a performance reason.
  template<bool kUseTail = true>
  class SlotFreeList {
   public:
    SlotFreeList() : head_(0U), tail_(0), size_(0) {}
    Slot* Head() const {
      return reinterpret_cast<Slot*>(head_);
    }
    Slot* Tail() const {
      CHECK(kUseTail);
      return reinterpret_cast<Slot*>(tail_);
    }
    size_t Size() const {
      return size_;
    }
    // Removes from the head of the free list.
    Slot* Remove() {
      Slot* slot;
      if (kIsDebugBuild) {
        Verify();
      }
      Slot** headp = reinterpret_cast<Slot**>(&head_);
      Slot** tailp = kUseTail ? reinterpret_cast<Slot**>(&tail_) : nullptr;
      Slot* old_head = *headp;
      if (old_head == nullptr) {
        // List was empty.
        if (kUseTail) {
          DCHECK(*tailp == nullptr);
        }
        return nullptr;
      } else {
        // List wasn't empty.
        if (kUseTail) {
          DCHECK(*tailp != nullptr);
        }
        Slot* old_head_next = old_head->Next();
        slot = old_head;
        *headp = old_head_next;
        if (kUseTail && old_head_next == nullptr) {
          // List becomes empty.
          *tailp = nullptr;
        }
      }
      slot->Clear();
      --size_;
      if (kIsDebugBuild) {
        Verify();
      }
      return slot;
    }
    void Add(Slot* slot) {
      if (kIsDebugBuild) {
        Verify();
      }
      DCHECK(slot != nullptr);
      DCHECK(slot->Next() == nullptr);
      Slot** headp = reinterpret_cast<Slot**>(&head_);
      Slot** tailp = kUseTail ? reinterpret_cast<Slot**>(&tail_) : nullptr;
      Slot* old_head = *headp;
      if (old_head == nullptr) {
        // List was empty.
        if (kUseTail) {
          DCHECK(*tailp == nullptr);
        }
        *headp = slot;
        if (kUseTail) {
          *tailp = slot;
        }
      } else {
        // List wasn't empty.
        if (kUseTail) {
          DCHECK(*tailp != nullptr);
        }
        *headp = slot;
        slot->SetNext(old_head);
      }
      ++size_;
      if (kIsDebugBuild) {
        Verify();
      }
    }
    // Merge the given list into this list. Empty the given list.
    // Deliberately support only a kUseTail == true SlotFreeList parameter because 1) we don't
    // currently have a situation where we need a kUseTail == false SlotFreeList parameter, and 2)
    // supporting the kUseTail == false parameter would require a O(n) linked list traversal to do
    // the merge if 'this' SlotFreeList has kUseTail == false, which we'd like to avoid.
    void Merge(SlotFreeList<true>* list) {
      if (kIsDebugBuild) {
        Verify();
        CHECK(list != nullptr);
        list->Verify();
      }
      if (list->Size() == 0) {
        return;
      }
      Slot** headp = reinterpret_cast<Slot**>(&head_);
      Slot** tailp = kUseTail ? reinterpret_cast<Slot**>(&tail_) : nullptr;
      Slot* old_head = *headp;
      if (old_head == nullptr) {
        // List was empty.
        *headp = list->Head();
        if (kUseTail) {
          *tailp = list->Tail();
        }
        size_ = list->Size();
      } else {
        // List wasn't empty.
        DCHECK(list->Head() != nullptr);
        *headp = list->Head();
        DCHECK(list->Tail() != nullptr);
        list->Tail()->SetNext(old_head);
        // if kUseTail, no change to tailp.
        size_ += list->Size();
      }
      list->Reset();
      if (kIsDebugBuild) {
        Verify();
      }
    }

    void Reset() {
      head_ = 0;
      if (kUseTail) {
        tail_ = 0;
      }
      size_ = 0;
    }

    void Verify() {
      Slot* head = reinterpret_cast<Slot*>(head_);
      Slot* tail = kUseTail ? reinterpret_cast<Slot*>(tail_) : nullptr;
      if (size_ == 0) {
        CHECK(head == nullptr);
        if (kUseTail) {
          CHECK(tail == nullptr);
        }
      } else {
        CHECK(head != nullptr);
        if (kUseTail) {
          CHECK(tail != nullptr);
        }
        size_t count = 0;
        for (Slot* slot = head; slot != nullptr; slot = slot->Next()) {
          ++count;
          if (kUseTail && slot->Next() == nullptr) {
            CHECK_EQ(slot, tail);
          }
        }
        CHECK_EQ(size_, count);
      }
    }

   private:
    // A pointer (Slot*) to the head of the list. Always 8 bytes so that we will have the same
    // layout between 32 bit and 64 bit, which is not strictly necessary, but we do so for 1)
    // uniformity, 2) we won't need to change this code if we move to a non-low 4G heap in the
    // future, and 3) the space savings by using 32 bit fields in 32 bit would be lost in noise
    // (won't open up enough space to cause an extra slot to be available).
    uint64_t head_;
    // A pointer (Slot*) to the tail of the list. Always 8 bytes so that we will have the same
    // layout between 32 bit and 64 bit. The tail is stored to speed up merging of lists.
    // Unused if kUseTail is false.
    uint64_t tail_;
    // The number of slots in the list. This is used to make it fast to check if a free list is all
    // free without traversing the whole free list.
    uint32_t size_;
    uint32_t padding_ ATTRIBUTE_UNUSED;
    friend class RosAlloc;
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
  // |                   |
  // | free list         |
  // |                   |
  // +-------------------+
  // |                   |
  // | bulk free list    |
  // |                   |
  // +-------------------+
  // |                   |
  // | thread-local free |
  // | list              |
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
    uint32_t padding_ ATTRIBUTE_UNUSED;
    // Use a tailless free list for free_list_ so that the alloc fast path does not manage the tail.
    SlotFreeList<false> free_list_;
    SlotFreeList<true> bulk_free_list_;
    SlotFreeList<true> thread_local_free_list_;
    // Padding due to alignment
    // Slot 0
    // Slot 1
    // ...

    // Returns the byte size of the header.
    static size_t fixed_header_size() {
      return sizeof(Run);
    }
    Slot* FirstSlot() const {
      const uint8_t idx = size_bracket_idx_;
      return reinterpret_cast<Slot*>(reinterpret_cast<uintptr_t>(this) + headerSizes[idx]);
    }
    Slot* LastSlot() {
      const uint8_t idx = size_bracket_idx_;
      const size_t bracket_size = bracketSizes[idx];
      uintptr_t end = reinterpret_cast<uintptr_t>(End());
      Slot* last_slot = reinterpret_cast<Slot*>(end - bracket_size);
      DCHECK_LE(FirstSlot(), last_slot);
      return last_slot;
    }
    SlotFreeList<false>* FreeList() {
      return &free_list_;
    }
    SlotFreeList<true>* BulkFreeList() {
      return &bulk_free_list_;
    }
    SlotFreeList<true>* ThreadLocalFreeList() {
      return &thread_local_free_list_;
    }
    void* End() {
      return reinterpret_cast<uint8_t*>(this) + kPageSize * numOfPages[size_bracket_idx_];
    }
    void SetIsThreadLocal(bool is_thread_local) {
      is_thread_local_  = is_thread_local ? 1 : 0;
    }
    bool IsThreadLocal() const {
      return is_thread_local_ != 0;
    }
    // Set up the free list for a new/empty run.
    void InitFreeList() {
      const uint8_t idx = size_bracket_idx_;
      const size_t bracket_size = bracketSizes[idx];
      Slot* first_slot = FirstSlot();
      // Add backwards so the first slot is at the head of the list.
      for (Slot* slot = LastSlot(); slot >= first_slot; slot = slot->Left(bracket_size)) {
        free_list_.Add(slot);
      }
    }
    // Merge the thread local free list to the free list.  Used when a thread-local run becomes
    // full.
    bool MergeThreadLocalFreeListToFreeList(bool* is_all_free_after_out);
    // Merge the bulk free list to the free list. Used in a bulk free.
    void MergeBulkFreeListToFreeList();
    // Merge the bulk free list to the thread local free list. In a bulk free, as a two-step
    // process, GC will first record all the slots to free in a run in the bulk free list where it
    // can write without a lock, and later acquire a lock once per run to merge the bulk free list
    // to the thread-local free list.
    void MergeBulkFreeListToThreadLocalFreeList();
    // Allocates a slot in a run.
    ALWAYS_INLINE void* AllocSlot();
    // Frees a slot in a run. This is used in a non-bulk free.
    void FreeSlot(void* ptr);
    // Add the given slot to the bulk free list. Returns the bracket size.
    size_t AddToBulkFreeList(void* ptr);
    // Add the given slot to the thread-local free list.
    void AddToThreadLocalFreeList(void* ptr);
    // Returns true if all the slots in the run are not in use.
    bool IsAllFree() const {
      return free_list_.Size() == numOfSlots[size_bracket_idx_];
    }
    // Returns the number of free slots.
    size_t NumberOfFreeSlots() {
      return free_list_.Size();
    }
    // Returns true if all the slots in the run are in use.
    ALWAYS_INLINE bool IsFull();
    // Returns true if the bulk free list is empty.
    bool IsBulkFreeListEmpty() const {
      return bulk_free_list_.Size() == 0;
    }
    // Returns true if the thread local free list is empty.
    bool IsThreadLocalFreeListEmpty() const {
      return thread_local_free_list_.Size() == 0;
    }
    // Zero the run's data.
    void ZeroData();
    // Zero the run's header and the slot headers.
    void ZeroHeaderAndSlotHeaders();
    // Iterate over all the slots and apply the given function.
    void InspectAllSlots(void (*handler)(void* start, void* end, size_t used_bytes, void* callback_arg), void* arg);
    // Dump the run metadata for debugging.
    std::string Dump();
    // Verify for debugging.
    void Verify(Thread* self, RosAlloc* rosalloc, bool running_on_memory_tool)
        REQUIRES(Locks::mutator_lock_)
        REQUIRES(Locks::thread_list_lock_);

   private:
    // The common part of AddToBulkFreeList() and AddToThreadLocalFreeList(). Returns the bracket
    // size.
    size_t AddToFreeListShared(void* ptr, SlotFreeList<true>* free_list, const char* caller_name);
    // Turns a FreeList into a string for debugging.
    template<bool kUseTail>
    std::string FreeListToStr(SlotFreeList<kUseTail>* free_list);
    // Check a given pointer is a valid slot address and return it as Slot*.
    Slot* ToSlot(void* ptr) {
      const uint8_t idx = size_bracket_idx_;
      const size_t bracket_size = bracketSizes[idx];
      const size_t offset_from_slot_base = reinterpret_cast<uint8_t*>(ptr)
          - reinterpret_cast<uint8_t*>(FirstSlot());
      DCHECK_EQ(offset_from_slot_base % bracket_size, static_cast<size_t>(0));
      size_t slot_idx = offset_from_slot_base / bracket_size;
      DCHECK_LT(slot_idx, numOfSlots[idx]);
      return reinterpret_cast<Slot*>(ptr);
    }
    size_t SlotIndex(Slot* slot) const {
      const uint8_t idx = size_bracket_idx_;
      const size_t bracket_size = bracketSizes[idx];
      const size_t offset_from_slot_base = reinterpret_cast<uint8_t*>(slot)
          - reinterpret_cast<uint8_t*>(FirstSlot());
      DCHECK_EQ(offset_from_slot_base % bracket_size, 0U);
      size_t slot_idx = offset_from_slot_base / bracket_size;
      DCHECK_LT(slot_idx, numOfSlots[idx]);
      return slot_idx;
    }

    // TODO: DISALLOW_COPY_AND_ASSIGN(Run);
  };

  // The magic number for a run.
  static constexpr uint8_t kMagicNum = 42;
  // The magic number for free pages.
  static constexpr uint8_t kMagicNumFree = 43;
  // The number of size brackets.
  static constexpr size_t kNumOfSizeBrackets = 42;
  // The sizes (the slot sizes, in bytes) of the size brackets.
  static size_t bracketSizes[kNumOfSizeBrackets];
  // The numbers of pages that are used for runs for each size bracket.
  static size_t numOfPages[kNumOfSizeBrackets];
  // The numbers of slots of the runs for each size bracket.
  static size_t numOfSlots[kNumOfSizeBrackets];
  // The header sizes in bytes of the runs for each size bracket.
  static size_t headerSizes[kNumOfSizeBrackets];

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
    DCHECK(8 <= size &&
           ((size <= kMaxThreadLocalBracketSize && size % kThreadLocalBracketQuantumSize == 0) ||
            (size <= kMaxRegularBracketSize && size % kBracketQuantumSize == 0) ||
            size == 1 * KB || size == 2 * KB));
    size_t idx;
    if (UNLIKELY(size == 1 * KB)) {
      idx = kNumOfSizeBrackets - 2;
    } else if (UNLIKELY(size == 2 * KB)) {
      idx = kNumOfSizeBrackets - 1;
    } else if (LIKELY(size <= kMaxThreadLocalBracketSize)) {
      DCHECK_EQ(size % kThreadLocalBracketQuantumSize, 0U);
      idx = size / kThreadLocalBracketQuantumSize - 1;
    } else {
      DCHECK(size <= kMaxRegularBracketSize);
      DCHECK_EQ((size - kMaxThreadLocalBracketSize) % kBracketQuantumSize, 0U);
      idx = ((size - kMaxThreadLocalBracketSize) / kBracketQuantumSize - 1)
          + kNumThreadLocalSizeBrackets;
    }
    DCHECK(bracketSizes[idx] == size);
    return idx;
  }
  // Returns true if the given allocation size is for a thread local allocation.
  static bool IsSizeForThreadLocal(size_t size) {
    bool is_size_for_thread_local = size <= kMaxThreadLocalBracketSize;
    DCHECK(size > kLargeSizeThreshold ||
           (is_size_for_thread_local == (SizeToIndex(size) < kNumThreadLocalSizeBrackets)));
    return is_size_for_thread_local;
  }
  // Rounds up the size up the nearest bracket size.
  static size_t RoundToBracketSize(size_t size) {
    DCHECK(size <= kLargeSizeThreshold);
    if (LIKELY(size <= kMaxThreadLocalBracketSize)) {
      return RoundUp(size, kThreadLocalBracketQuantumSize);
    } else if (size <= kMaxRegularBracketSize) {
      return RoundUp(size, kBracketQuantumSize);
    } else if (UNLIKELY(size <= 1 * KB)) {
      return 1 * KB;
    } else {
      DCHECK_LE(size, 2 * KB);
      return 2 * KB;
    }
  }
  // Returns the size bracket index from the byte size with rounding.
  static size_t SizeToIndex(size_t size) {
    DCHECK(size <= kLargeSizeThreshold);
    if (LIKELY(size <= kMaxThreadLocalBracketSize)) {
      return RoundUp(size, kThreadLocalBracketQuantumSize) / kThreadLocalBracketQuantumSize - 1;
    } else if (size <= kMaxRegularBracketSize) {
      return (RoundUp(size, kBracketQuantumSize) - kMaxThreadLocalBracketSize) / kBracketQuantumSize
          - 1 + kNumThreadLocalSizeBrackets;
    } else if (size <= 1 * KB) {
      return kNumOfSizeBrackets - 2;
    } else {
      DCHECK_LE(size, 2 * KB);
      return kNumOfSizeBrackets - 1;
    }
  }
  // A combination of SizeToIndex() and RoundToBracketSize().
  static size_t SizeToIndexAndBracketSize(size_t size, size_t* bracket_size_out) {
    DCHECK(size <= kLargeSizeThreshold);
    size_t idx;
    size_t bracket_size;
    if (LIKELY(size <= kMaxThreadLocalBracketSize)) {
      bracket_size = RoundUp(size, kThreadLocalBracketQuantumSize);
      idx = bracket_size / kThreadLocalBracketQuantumSize - 1;
    } else if (size <= kMaxRegularBracketSize) {
      bracket_size = RoundUp(size, kBracketQuantumSize);
      idx = ((bracket_size - kMaxThreadLocalBracketSize) / kBracketQuantumSize - 1)
          + kNumThreadLocalSizeBrackets;
    } else if (size <= 1 * KB) {
      bracket_size = 1 * KB;
      idx = kNumOfSizeBrackets - 2;
    } else {
      DCHECK(size <= 2 * KB);
      bracket_size = 2 * KB;
      idx = kNumOfSizeBrackets - 1;
    }
    DCHECK_EQ(idx, SizeToIndex(size)) << idx;
    DCHECK_EQ(bracket_size, IndexToBracketSize(idx)) << idx;
    DCHECK_EQ(bracket_size, bracketSizes[idx]) << idx;
    DCHECK_LE(size, bracket_size) << idx;
    DCHECK(size > kMaxRegularBracketSize ||
           (size <= kMaxThreadLocalBracketSize &&
            bracket_size - size < kThreadLocalBracketQuantumSize) ||
           (size <= kMaxRegularBracketSize && bracket_size - size < kBracketQuantumSize)) << idx;
    *bracket_size_out = bracket_size;
    return idx;
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

  // We use thread-local runs for the size brackets whose indexes
  // are less than this index. We use shared (current) runs for the rest.
  // Sync this with the length of Thread::rosalloc_runs_.
  static const size_t kNumThreadLocalSizeBrackets = 16;
  static_assert(kNumThreadLocalSizeBrackets == kNumRosAllocThreadLocalSizeBracketsInThread,
                "Mismatch between kNumThreadLocalSizeBrackets and "
                "kNumRosAllocThreadLocalSizeBracketsInThread");

  // The size of the largest bracket we use thread-local runs for.
  // This should be equal to bracketSizes[kNumThreadLocalSizeBrackets - 1].
  static const size_t kMaxThreadLocalBracketSize = 128;

  // We use regular (8 or 16-bytes increment) runs for the size brackets whose indexes are less than
  // this index.
  static const size_t kNumRegularSizeBrackets = 40;

  // The size of the largest regular (8 or 16-byte increment) bracket. Non-regular brackets are the
  // 1 KB and the 2 KB brackets. This should be equal to bracketSizes[kNumRegularSizeBrackets - 1].
  static const size_t kMaxRegularBracketSize = 512;

  // The bracket size increment for the thread-local brackets (<= kMaxThreadLocalBracketSize bytes).
  static constexpr size_t kThreadLocalBracketQuantumSize = 8;

  // Equal to Log2(kThreadLocalBracketQuantumSize).
  static constexpr size_t kThreadLocalBracketQuantumSizeShift = 3;

  // The bracket size increment for the non-thread-local, regular brackets (of size <=
  // kMaxRegularBracketSize bytes and > kMaxThreadLocalBracketSize bytes).
  static constexpr size_t kBracketQuantumSize = 16;

  // Equal to Log2(kBracketQuantumSize).
  static constexpr size_t kBracketQuantumSizeShift = 4;

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
  // RevokeThreadLocalRuns() on the bulk free list.
  ReaderWriterMutex bulk_free_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // The page release mode.
  const PageReleaseMode page_release_mode_;
  // Under kPageReleaseModeSize(AndEnd), if the free page run size is
  // greater than or equal to this value, release pages.
  const size_t page_release_size_threshold_;

  // Whether this allocator is running under Valgrind.
  bool is_running_on_memory_tool_;

  // The base address of the memory region that's managed by this allocator.
  uint8_t* Begin() { return base_; }
  // The end address of the memory region that's managed by this allocator.
  uint8_t* End() { return base_ + capacity_; }

  // Page-granularity alloc/free
  void* AllocPages(Thread* self, size_t num_pages, uint8_t page_map_type)
      REQUIRES(lock_);
  // Returns how many bytes were freed.
  size_t FreePages(Thread* self, void* ptr, bool already_zero) REQUIRES(lock_);

  // Allocate/free a run slot.
  void* AllocFromRun(Thread* self, size_t size, size_t* bytes_allocated, size_t* usable_size,
                     size_t* bytes_tl_bulk_allocated)
      REQUIRES(!lock_);
  // Allocate/free a run slot without acquiring locks.
  // TODO: REQUIRES(Locks::mutator_lock_)
  void* AllocFromRunThreadUnsafe(Thread* self, size_t size, size_t* bytes_allocated,
                                 size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      REQUIRES(!lock_);
  void* AllocFromCurrentRunUnlocked(Thread* self, size_t idx) REQUIRES(!lock_);

  // Returns the bracket size.
  size_t FreeFromRun(Thread* self, void* ptr, Run* run)
      REQUIRES(!lock_);

  // Used to allocate a new thread local run for a size bracket.
  Run* AllocRun(Thread* self, size_t idx) REQUIRES(!lock_);

  // Used to acquire a new/reused run for a size bracket. Used when a
  // thread-local or current run gets full.
  Run* RefillRun(Thread* self, size_t idx) REQUIRES(!lock_);

  // The internal of non-bulk Free().
  size_t FreeInternal(Thread* self, void* ptr) REQUIRES(!lock_);

  // Allocates large objects.
  void* AllocLargeObject(Thread* self, size_t size, size_t* bytes_allocated,
                         size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      REQUIRES(!lock_);

  // Revoke a run by adding it to non_full_runs_ or freeing the pages.
  void RevokeRun(Thread* self, size_t idx, Run* run) REQUIRES(!lock_);

  // Revoke the current runs which share an index with the thread local runs.
  void RevokeThreadUnsafeCurrentRuns() REQUIRES(!lock_);

  // Release a range of pages.
  size_t ReleasePageRange(uint8_t* start, uint8_t* end) REQUIRES(lock_);

  // Dumps the page map for debugging.
  std::string DumpPageMap() REQUIRES(lock_);

 public:
  RosAlloc(void* base, size_t capacity, size_t max_capacity,
           PageReleaseMode page_release_mode,
           bool running_on_memory_tool,
           size_t page_release_size_threshold = kDefaultPageReleaseSizeThreshold);
  ~RosAlloc();

  static size_t RunFreeListOffset() {
    return OFFSETOF_MEMBER(Run, free_list_);
  }
  static size_t RunFreeListHeadOffset() {
    return OFFSETOF_MEMBER(SlotFreeList<false>, head_);
  }
  static size_t RunFreeListSizeOffset() {
    return OFFSETOF_MEMBER(SlotFreeList<false>, size_);
  }
  static size_t RunSlotNextOffset() {
    return OFFSETOF_MEMBER(Slot, next_);
  }

  // If kThreadUnsafe is true then the allocator may avoid acquiring some locks as an optimization.
  // If used, this may cause race conditions if multiple threads are allocating at the same time.
  template<bool kThreadSafe = true>
  void* Alloc(Thread* self, size_t size, size_t* bytes_allocated, size_t* usable_size,
              size_t* bytes_tl_bulk_allocated)
      REQUIRES(!lock_);
  size_t Free(Thread* self, void* ptr)
      REQUIRES(!bulk_free_lock_, !lock_);
  size_t BulkFree(Thread* self, void** ptrs, size_t num_ptrs)
      REQUIRES(!bulk_free_lock_, !lock_);

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
  size_t UsableSize(const void* ptr) REQUIRES(!lock_);
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
  bool Trim() REQUIRES(!lock_);
  // Iterates over all the memory slots and apply the given function.
  void InspectAll(void (*handler)(void* start, void* end, size_t used_bytes, void* callback_arg),
                  void* arg)
      REQUIRES(!lock_);

  // Release empty pages.
  size_t ReleasePages() REQUIRES(!lock_);
  // Returns the current footprint.
  size_t Footprint() REQUIRES(!lock_);
  // Returns the current capacity, maximum footprint.
  size_t FootprintLimit() REQUIRES(!lock_);
  // Update the current capacity.
  void SetFootprintLimit(size_t bytes) REQUIRES(!lock_);

  // Releases the thread-local runs assigned to the given thread back to the common set of runs.
  // Returns the total bytes of free slots in the revoked thread local runs. This is to be
  // subtracted from Heap::num_bytes_allocated_ to cancel out the ahead-of-time counting.
  size_t RevokeThreadLocalRuns(Thread* thread) REQUIRES(!lock_, !bulk_free_lock_);
  // Releases the thread-local runs assigned to all the threads back to the common set of runs.
  // Returns the total bytes of free slots in the revoked thread local runs. This is to be
  // subtracted from Heap::num_bytes_allocated_ to cancel out the ahead-of-time counting.
  size_t RevokeAllThreadLocalRuns() REQUIRES(!Locks::thread_list_lock_, !lock_, !bulk_free_lock_);
  // Assert the thread local runs of a thread are revoked.
  void AssertThreadLocalRunsAreRevoked(Thread* thread) REQUIRES(!bulk_free_lock_);
  // Assert all the thread local runs are revoked.
  void AssertAllThreadLocalRunsAreRevoked() REQUIRES(!Locks::thread_list_lock_, !bulk_free_lock_);

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
  void Verify() REQUIRES(Locks::mutator_lock_, !Locks::thread_list_lock_, !bulk_free_lock_,
                         !lock_);

  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes)
      REQUIRES(!bulk_free_lock_, !lock_);

  void DumpStats(std::ostream& os)
      REQUIRES(Locks::mutator_lock_) REQUIRES(!lock_) REQUIRES(!bulk_free_lock_);

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
