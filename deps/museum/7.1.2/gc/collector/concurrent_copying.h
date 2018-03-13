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
#include "immune_spaces.h"
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

class ConcurrentCopying : public GarbageCollector {
 public:
  // TODO: disable thse flags for production use.
  // Enable the no-from-space-refs verification at the pause.
  static constexpr bool kEnableNoFromSpaceRefsVerification = true;
  // Enable the from-space bytes/objects check.
  static constexpr bool kEnableFromSpaceAccountingCheck = true;
  // Enable verbose mode.
  static constexpr bool kVerboseMode = false;

  ConcurrentCopying(Heap* heap, const std::string& name_prefix = "");
  ~ConcurrentCopying();

  virtual void RunPhases() OVERRIDE REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  void InitializePhase() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void MarkingPhase() SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  void ReclaimPhase() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void FinishPhase() REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);

  void BindBitmaps() SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::heap_bitmap_lock_);
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
      SHARED_REQUIRES(Locks::mutator_lock_);
  void AssertToSpaceInvariant(GcRootSource* gc_root_source, mirror::Object* ref)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool IsInToSpace(mirror::Object* ref) SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(ref != nullptr);
    return IsMarked(ref) == ref;
  }
  ALWAYS_INLINE mirror::Object* Mark(mirror::Object* from_ref) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  bool IsMarking() const {
    return is_marking_;
  }
  bool IsActive() const {
    return is_active_;
  }
  Barrier& GetBarrier() {
    return *gc_barrier_;
  }
  bool IsWeakRefAccessEnabled() {
    return weak_ref_access_enabled_.LoadRelaxed();
  }
  void RevokeThreadLocalMarkStack(Thread* thread) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);

 private:
  void PushOntoMarkStack(mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  mirror::Object* Copy(mirror::Object* from_ref) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!skipped_blocks_lock_, !mark_stack_lock_);
  void Scan(mirror::Object* to_ref) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void Process(mirror::Object* obj, MemberOffset offset)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_ , !skipped_blocks_lock_);
  virtual void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info)
      OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  void MarkRoot(mirror::CompressedReference<mirror::Object>* root)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  virtual void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                          const RootInfo& info)
      OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  void VerifyNoFromSpaceReferences() REQUIRES(Locks::mutator_lock_);
  accounting::ObjectStack* GetAllocationStack();
  accounting::ObjectStack* GetLiveStack();
  virtual void ProcessMarkStack() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  bool ProcessMarkStackOnce() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void ProcessMarkStackRef(mirror::Object* to_ref) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  size_t ProcessThreadLocalMarkStacks(bool disable_weak_ref_access)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void RevokeThreadLocalMarkStacks(bool disable_weak_ref_access)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void SwitchToSharedMarkStackMode() SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void SwitchToGcExclusiveMarkStackMode() SHARED_REQUIRES(Locks::mutator_lock_);
  virtual void DelayReferenceReferent(mirror::Class* klass, mirror::Reference* reference) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_);
  void ProcessReferences(Thread* self) SHARED_REQUIRES(Locks::mutator_lock_);
  virtual mirror::Object* MarkObject(mirror::Object* from_ref) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  virtual void MarkHeapReference(mirror::HeapReference<mirror::Object>* from_ref) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  virtual mirror::Object* IsMarked(mirror::Object* from_ref) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_);
  virtual bool IsMarkedHeapReference(mirror::HeapReference<mirror::Object>* field) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_);
  void SweepSystemWeaks(Thread* self)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Locks::heap_bitmap_lock_);
  void Sweep(bool swap_bitmaps)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_, !mark_stack_lock_);
  void SweepLargeObjects(bool swap_bitmaps)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);
  void ClearBlackPtrs()
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);
  void FillWithDummyObject(mirror::Object* dummy_obj, size_t byte_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::Object* AllocateInSkippedBlock(size_t alloc_size)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!skipped_blocks_lock_);
  void CheckEmptyMarkStack() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void IssueEmptyCheckpoint() SHARED_REQUIRES(Locks::mutator_lock_);
  bool IsOnAllocStack(mirror::Object* ref) SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::Object* GetFwdPtr(mirror::Object* from_ref)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void FlipThreadRoots() REQUIRES(!Locks::mutator_lock_);
  void SwapStacks() SHARED_REQUIRES(Locks::mutator_lock_);
  void RecordLiveStackFreezeSize(Thread* self);
  void ComputeUnevacFromSpaceLiveRatio();
  void LogFromSpaceRefHolder(mirror::Object* obj, MemberOffset offset)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void AssertToSpaceInvariantInNonMovingSpace(mirror::Object* obj, mirror::Object* ref)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void ReenableWeakRefAccess(Thread* self) SHARED_REQUIRES(Locks::mutator_lock_);
  void DisableMarking() SHARED_REQUIRES(Locks::mutator_lock_);
  void IssueDisableMarkingCheckpoint() SHARED_REQUIRES(Locks::mutator_lock_);
  void ExpandGcMarkStack() SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::Object* MarkNonMoving(mirror::Object* from_ref) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);

  space::RegionSpace* region_space_;      // The underlying region space.
  std::unique_ptr<Barrier> gc_barrier_;
  std::unique_ptr<accounting::ObjectStack> gc_mark_stack_;
  Mutex mark_stack_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::vector<accounting::ObjectStack*> revoked_mark_stacks_
      GUARDED_BY(mark_stack_lock_);
  static constexpr size_t kMarkStackSize = kPageSize;
  static constexpr size_t kMarkStackPoolSize = 256;
  std::vector<accounting::ObjectStack*> pooled_mark_stacks_
      GUARDED_BY(mark_stack_lock_);
  Thread* thread_running_gc_;
  bool is_marking_;                       // True while marking is ongoing.
  bool is_active_;                        // True while the collection is ongoing.
  bool is_asserting_to_space_invariant_;  // True while asserting the to-space invariant.
  ImmuneSpaces immune_spaces_;
  std::unique_ptr<accounting::HeapBitmap> cc_heap_bitmap_;
  std::vector<accounting::SpaceBitmap<kObjectAlignment>*> cc_bitmaps_;
  accounting::SpaceBitmap<kObjectAlignment>* region_space_bitmap_;
  // A cache of Heap::GetMarkBitmap().
  accounting::HeapBitmap* heap_mark_bitmap_;
  size_t live_stack_freeze_size_;
  size_t from_space_num_objects_at_first_pause_;
  size_t from_space_num_bytes_at_first_pause_;
  Atomic<int> is_mark_stack_push_disallowed_;
  enum MarkStackMode {
    kMarkStackModeOff = 0,      // Mark stack is off.
    kMarkStackModeThreadLocal,  // All threads except for the GC-running thread push refs onto
                                // thread-local mark stacks. The GC-running thread pushes onto and
                                // pops off the GC mark stack without a lock.
    kMarkStackModeShared,       // All threads share the GC mark stack with a lock.
    kMarkStackModeGcExclusive   // The GC-running thread pushes onto and pops from the GC mark stack
                                // without a lock. Other threads won't access the mark stack.
  };
  Atomic<MarkStackMode> mark_stack_mode_;
  Atomic<bool> weak_ref_access_enabled_;

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

  class AssertToSpaceInvariantFieldVisitor;
  class AssertToSpaceInvariantObjectVisitor;
  class AssertToSpaceInvariantRefsVisitor;
  class ClearBlackPtrsVisitor;
  class ComputeUnevacFromSpaceLiveRatioVisitor;
  class DisableMarkingCheckpoint;
  class FlipCallback;
  class ImmuneSpaceObjVisitor;
  class LostCopyVisitor;
  class RefFieldsVisitor;
  class RevokeThreadLocalMarkStackCheckpoint;
  class VerifyNoFromSpaceRefsFieldVisitor;
  class VerifyNoFromSpaceRefsObjectVisitor;
  class VerifyNoFromSpaceRefsVisitor;
  class ThreadFlipVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ConcurrentCopying);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
