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

#ifndef ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
#define ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_

#include "barrier.h"
#include "garbage_collector.h"
#include "immune_region.h"
#include "jni.h"
#include "object_callbacks.h"
#include "offsets.h"
#include "gc/accounting/atomic_stack.h"
#include "gc/accounting/read_barrier_table.h"
#include "gc/accounting/space_bitmap.h"
#include "mirror/object.h"
#include "mirror/object_reference.h"
#include "safe_map.h"

#include <unordered_map>
#include <vector>

namespace art {
class RootInfo;

namespace gc {

namespace accounting {
  typedef SpaceBitmap<kObjectAlignment> ContinuousSpaceBitmap;
  class HeapBitmap;
}  // namespace accounting

namespace space {
  class RegionSpace;
}  // namespace space

namespace collector {

// Concurrent queue. Used as the mark stack. TODO: use a concurrent
// stack for locality.
class MarkQueue {
 public:
  explicit MarkQueue(size_t size) : size_(size) {
    CHECK(IsPowerOfTwo(size_));
    buf_.reset(new Atomic<mirror::Object*>[size_]);
    CHECK(buf_.get() != nullptr);
    Clear();
  }

  ALWAYS_INLINE Atomic<mirror::Object*>* GetSlotAddr(size_t index) {
    return &(buf_.get()[index & (size_ - 1)]);
  }

  // Multiple-proceducer enqueue.
  bool Enqueue(mirror::Object* to_ref) {
    size_t t;
    do {
      t = tail_.LoadRelaxed();
      size_t h = head_.LoadSequentiallyConsistent();
      if (t + size_ == h) {
        // It's full.
        return false;
      }
    } while (!tail_.CompareExchangeWeakSequentiallyConsistent(t, t + 1));
    // We got a slot but its content has not been filled yet at this point.
    GetSlotAddr(t)->StoreSequentiallyConsistent(to_ref);
    return true;
  }

  // Thread-unsafe.
  bool EnqueueThreadUnsafe(mirror::Object* to_ref) {
    size_t t = tail_.LoadRelaxed();
    size_t h = head_.LoadRelaxed();
    if (t + size_ == h) {
      // It's full.
      return false;
    }
    GetSlotAddr(t)->StoreRelaxed(to_ref);
    tail_.StoreRelaxed(t + 1);
    return true;
  }

  // Single-consumer dequeue.
  mirror::Object* Dequeue() {
    size_t h = head_.LoadRelaxed();
    size_t t = tail_.LoadSequentiallyConsistent();
    if (h == t) {
      // it's empty.
      return nullptr;
    }
    Atomic<mirror::Object*>* slot = GetSlotAddr(h);
    mirror::Object* ref = slot->LoadSequentiallyConsistent();
    while (ref == nullptr) {
      // Wait until the slot content becomes visible.
      ref = slot->LoadSequentiallyConsistent();
    }
    slot->StoreRelaxed(nullptr);
    head_.StoreSequentiallyConsistent(h + 1);
    return ref;
  }

  bool IsEmpty() {
    size_t h = head_.LoadSequentiallyConsistent();
    size_t t = tail_.LoadSequentiallyConsistent();
    return h == t;
  }

  void Clear() {
    head_.StoreRelaxed(0);
    tail_.StoreRelaxed(0);
    memset(buf_.get(), 0, size_ * sizeof(Atomic<mirror::Object*>));
  }

 private:
  Atomic<size_t> head_;
  Atomic<size_t> tail_;

  size_t size_;
  std::unique_ptr<Atomic<mirror::Object*>[]> buf_;
};

class ConcurrentCopying : public GarbageCollector {
 public:
  // TODO: disable thse flags for production use.
  // Enable the no-from-space-refs verification at the pause.
  static constexpr bool kEnableNoFromSpaceRefsVerification = true;
  // Enable the from-space bytes/objects check.
  static constexpr bool kEnableFromSpaceAccountingCheck = true;
  // Enable verbose mode.
  static constexpr bool kVerboseMode = true;

  ConcurrentCopying(Heap* heap, const std::string& name_prefix = "");
  ~ConcurrentCopying();

  virtual void RunPhases() OVERRIDE;
  void InitializePhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void MarkingPhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ReclaimPhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void FinishPhase();

  void BindBitmaps() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::heap_bitmap_lock_);
  virtual GcType GetGcType() const OVERRIDE {
    return kGcTypePartial;
  }
  virtual CollectorType GetCollectorType() const OVERRIDE {
    return kCollectorTypeCC;
  }
  virtual void RevokeAllThreadLocalBuffers() OVERRIDE;
  void SetRegionSpace(space::RegionSpace* region_space) {
    DCHECK(region_space != nullptr);
    region_space_ = region_space;
  }
  space::RegionSpace* RegionSpace() {
    return region_space_;
  }
  void AssertToSpaceInvariant(mirror::Object* obj, MemberOffset offset, mirror::Object* ref)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsInToSpace(mirror::Object* ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(ref != nullptr);
    return IsMarked(ref) == ref;
  }
  mirror::Object* Mark(mirror::Object* from_ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsMarking() const {
    return is_marking_;
  }
  bool IsActive() const {
    return is_active_;
  }
  Barrier& GetBarrier() {
    return *gc_barrier_;
  }

 private:
  mirror::Object* PopOffMarkStack();
  template<bool kThreadSafe>
  void PushOntoMarkStack(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Object* Copy(mirror::Object* from_ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Scan(mirror::Object* to_ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Process(mirror::Object* obj, MemberOffset offset)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                          const RootInfo& info)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void VerifyNoFromSpaceReferences() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  accounting::ObjectStack* GetAllocationStack();
  accounting::ObjectStack* GetLiveStack();
  bool ProcessMarkStack() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DelayReferenceReferent(mirror::Class* klass, mirror::Reference* reference)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ProcessReferences(Thread* self, bool concurrent)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Object* IsMarked(mirror::Object* from_ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static mirror::Object* MarkCallback(mirror::Object* from_ref, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static mirror::Object* IsMarkedCallback(mirror::Object* from_ref, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static bool IsHeapReferenceMarkedCallback(
      mirror::HeapReference<mirror::Object>* field, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void ProcessMarkStackCallback(void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SweepSystemWeaks(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(Locks::heap_bitmap_lock_);
  void Sweep(bool swap_bitmaps)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  void SweepLargeObjects(bool swap_bitmaps)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  void ClearBlackPtrs()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  void FillWithDummyObject(mirror::Object* dummy_obj, size_t byte_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Object* AllocateInSkippedBlock(size_t alloc_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void CheckEmptyMarkQueue() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void IssueEmptyCheckpoint() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsOnAllocStack(mirror::Object* ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Object* GetFwdPtr(mirror::Object* from_ref)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void FlipThreadRoots() LOCKS_EXCLUDED(Locks::mutator_lock_);
  void SwapStacks(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void RecordLiveStackFreezeSize(Thread* self);
  void ComputeUnevacFromSpaceLiveRatio();

  space::RegionSpace* region_space_;      // The underlying region space.
  std::unique_ptr<Barrier> gc_barrier_;
  MarkQueue mark_queue_;
  bool is_marking_;                       // True while marking is ongoing.
  bool is_active_;                        // True while the collection is ongoing.
  bool is_asserting_to_space_invariant_;  // True while asserting the to-space invariant.
  ImmuneRegion immune_region_;
  std::unique_ptr<accounting::HeapBitmap> cc_heap_bitmap_;
  std::vector<accounting::SpaceBitmap<kObjectAlignment>*> cc_bitmaps_;
  accounting::SpaceBitmap<kObjectAlignment>* region_space_bitmap_;
  // A cache of Heap::GetMarkBitmap().
  accounting::HeapBitmap* heap_mark_bitmap_;
  size_t live_stack_freeze_size_;
  size_t from_space_num_objects_at_first_pause_;
  size_t from_space_num_bytes_at_first_pause_;
  Atomic<int> is_mark_queue_push_disallowed_;

  // How many objects and bytes we moved. Used for accounting.
  Atomic<size_t> bytes_moved_;
  Atomic<size_t> objects_moved_;

  // The skipped blocks are memory blocks/chucks that were copies of
  // objects that were unused due to lost races (cas failures) at
  // object copy/forward pointer install. They are reused.
  Mutex skipped_blocks_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::multimap<size_t, uint8_t*> skipped_blocks_map_ GUARDED_BY(skipped_blocks_lock_);
  Atomic<size_t> to_space_bytes_skipped_;
  Atomic<size_t> to_space_objects_skipped_;

  accounting::ReadBarrierTable* rb_table_;
  bool force_evacuate_all_;  // True if all regions are evacuated.

  friend class ConcurrentCopyingRefFieldsVisitor;
  friend class ConcurrentCopyingImmuneSpaceObjVisitor;
  friend class ConcurrentCopyingVerifyNoFromSpaceRefsVisitor;
  friend class ConcurrentCopyingVerifyNoFromSpaceRefsObjectVisitor;
  friend class ConcurrentCopyingClearBlackPtrsVisitor;
  friend class ConcurrentCopyingLostCopyVisitor;
  friend class ThreadFlipVisitor;
  friend class FlipCallback;
  friend class ConcurrentCopyingComputeUnevacFromSpaceLiveRatioVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ConcurrentCopying);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
