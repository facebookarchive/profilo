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
#include "gc/accounting/space_bitmap.h"
#include "mirror/object.h"
#include "mirror/object_reference.h"
#include "safe_map.h"

#include <unordered_map>
#include <vector>

namespace art {
class Closure;
class RootInfo;

namespace gc {

namespace accounting {
  template<typename T> class AtomicStack;
  typedef AtomicStack<mirror::Object> ObjectStack;
  typedef SpaceBitmap<kObjectAlignment> ContinuousSpaceBitmap;
  class HeapBitmap;
  class ReadBarrierTable;
}  // namespace accounting

namespace space {
  class RegionSpace;
}  // namespace space

namespace collector {

class ConcurrentCopying : public GarbageCollector {
 public:
  // Enable the no-from-space-refs verification at the pause.
  static constexpr bool kEnableNoFromSpaceRefsVerification = kIsDebugBuild;
  // Enable the from-space bytes/objects check.
  static constexpr bool kEnableFromSpaceAccountingCheck = kIsDebugBuild;
  // Enable verbose mode.
  static constexpr bool kVerboseMode = false;
  // If kGrayDirtyImmuneObjects is true then we gray dirty objects in the GC pause to prevent dirty
  // pages.
  static constexpr bool kGrayDirtyImmuneObjects = true;

  explicit ConcurrentCopying(Heap* heap,
                             const std::string& name_prefix = "",
                             bool measure_read_barrier_slow_path = false);
  ~ConcurrentCopying();

  virtual void RunPhases() OVERRIDE
      REQUIRES(!immune_gray_stack_lock_,
               !mark_stack_lock_,
               !rb_slow_path_histogram_lock_,
               !skipped_blocks_lock_);
  void InitializePhase() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !immune_gray_stack_lock_);
  void MarkingPhase() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void ReclaimPhase() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void FinishPhase() REQUIRES(!mark_stack_lock_,
                              !rb_slow_path_histogram_lock_,
                              !skipped_blocks_lock_);

  void BindBitmaps() REQUIRES_SHARED(Locks::mutator_lock_)
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
      REQUIRES_SHARED(Locks::mutator_lock_);
  void AssertToSpaceInvariant(GcRootSource* gc_root_source, mirror::Object* ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsInToSpace(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(ref != nullptr);
    return IsMarked(ref) == ref;
  }
  template<bool kGrayImmuneObject = true, bool kFromGCThread = false>
  ALWAYS_INLINE mirror::Object* Mark(mirror::Object* from_ref,
                                     mirror::Object* holder = nullptr,
                                     MemberOffset offset = MemberOffset(0))
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  ALWAYS_INLINE mirror::Object* MarkFromReadBarrier(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  bool IsMarking() const {
    return is_marking_;
  }
  bool IsActive() const {
    return is_active_;
  }
  Barrier& GetBarrier() {
    return *gc_barrier_;
  }
  bool IsWeakRefAccessEnabled() REQUIRES(Locks::thread_list_lock_) {
    return weak_ref_access_enabled_;
  }
  void RevokeThreadLocalMarkStack(Thread* thread) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);

  virtual mirror::Object* IsMarked(mirror::Object* from_ref) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void PushOntoMarkStack(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  mirror::Object* Copy(mirror::Object* from_ref,
                       mirror::Object* holder,
                       MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void Scan(mirror::Object* to_ref) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void Process(mirror::Object* obj, MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_ , !skipped_blocks_lock_, !immune_gray_stack_lock_);
  virtual void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  template<bool kGrayImmuneObject>
  void MarkRoot(mirror::CompressedReference<mirror::Object>* root)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  virtual void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                          const RootInfo& info)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void VerifyNoFromSpaceReferences() REQUIRES(Locks::mutator_lock_);
  accounting::ObjectStack* GetAllocationStack();
  accounting::ObjectStack* GetLiveStack();
  virtual void ProcessMarkStack() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  bool ProcessMarkStackOnce() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void ProcessMarkStackRef(mirror::Object* to_ref) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void GrayAllDirtyImmuneObjects()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void VerifyGrayImmuneObjects()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  static void VerifyNoMissingCardMarkCallback(mirror::Object* obj, void* arg)
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void VerifyNoMissingCardMarks()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  size_t ProcessThreadLocalMarkStacks(bool disable_weak_ref_access, Closure* checkpoint_callback)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void RevokeThreadLocalMarkStacks(bool disable_weak_ref_access, Closure* checkpoint_callback)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SwitchToSharedMarkStackMode() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void SwitchToGcExclusiveMarkStackMode() REQUIRES_SHARED(Locks::mutator_lock_);
  virtual void DelayReferenceReferent(ObjPtr<mirror::Class> klass,
                                      ObjPtr<mirror::Reference> reference) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ProcessReferences(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  virtual mirror::Object* MarkObject(mirror::Object* from_ref) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  virtual void MarkHeapReference(mirror::HeapReference<mirror::Object>* from_ref,
                                 bool do_atomic_update) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  bool IsMarkedInUnevacFromSpace(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  virtual bool IsNullOrMarkedHeapReference(mirror::HeapReference<mirror::Object>* field,
                                           bool do_atomic_update) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SweepSystemWeaks(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::heap_bitmap_lock_);
  void Sweep(bool swap_bitmaps)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_, !mark_stack_lock_);
  void SweepLargeObjects(bool swap_bitmaps)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);
  void MarkZygoteLargeObjects()
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FillWithDummyObject(mirror::Object* dummy_obj, size_t byte_size)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* AllocateInSkippedBlock(size_t alloc_size)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CheckEmptyMarkStack() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void IssueEmptyCheckpoint() REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsOnAllocStack(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* GetFwdPtr(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FlipThreadRoots() REQUIRES(!Locks::mutator_lock_);
  void SwapStacks() REQUIRES_SHARED(Locks::mutator_lock_);
  void RecordLiveStackFreezeSize(Thread* self);
  void ComputeUnevacFromSpaceLiveRatio();
  void LogFromSpaceRefHolder(mirror::Object* obj, MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void AssertToSpaceInvariantInNonMovingSpace(mirror::Object* obj, mirror::Object* ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ReenableWeakRefAccess(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void DisableMarking() REQUIRES_SHARED(Locks::mutator_lock_);
  void IssueDisableMarkingCheckpoint() REQUIRES_SHARED(Locks::mutator_lock_);
  void ExpandGcMarkStack() REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* MarkNonMoving(mirror::Object* from_ref,
                                mirror::Object* holder = nullptr,
                                MemberOffset offset = MemberOffset(0))
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  ALWAYS_INLINE mirror::Object* MarkUnevacFromSpaceRegion(mirror::Object* from_ref,
      accounting::SpaceBitmap<kObjectAlignment>* bitmap)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  template<bool kGrayImmuneObject>
  ALWAYS_INLINE mirror::Object* MarkImmuneSpace(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!immune_gray_stack_lock_);
  void PushOntoFalseGrayStack(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void ProcessFalseGrayStack() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void ScanImmuneObject(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  mirror::Object* MarkFromReadBarrierWithMeasurements(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void DumpPerformanceInfo(std::ostream& os) OVERRIDE REQUIRES(!rb_slow_path_histogram_lock_);

  space::RegionSpace* region_space_;      // The underlying region space.
  std::unique_ptr<Barrier> gc_barrier_;
  std::unique_ptr<accounting::ObjectStack> gc_mark_stack_;
  std::unique_ptr<accounting::ObjectStack> rb_mark_bit_stack_;
  bool rb_mark_bit_stack_full_;
  std::vector<mirror::Object*> false_gray_stack_ GUARDED_BY(mark_stack_lock_);
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
  bool weak_ref_access_enabled_ GUARDED_BY(Locks::thread_list_lock_);

  // How many objects and bytes we moved. Used for accounting.
  Atomic<size_t> bytes_moved_;
  Atomic<size_t> objects_moved_;
  Atomic<uint64_t> cumulative_bytes_moved_;
  Atomic<uint64_t> cumulative_objects_moved_;

  // The skipped blocks are memory blocks/chucks that were copies of
  // objects that were unused due to lost races (cas failures) at
  // object copy/forward pointer install. They are reused.
  Mutex skipped_blocks_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::multimap<size_t, uint8_t*> skipped_blocks_map_ GUARDED_BY(skipped_blocks_lock_);
  Atomic<size_t> to_space_bytes_skipped_;
  Atomic<size_t> to_space_objects_skipped_;

  // If measure_read_barrier_slow_path_ is true, we count how long is spent in MarkFromReadBarrier
  // and also log.
  bool measure_read_barrier_slow_path_;
  // mark_from_read_barrier_measurements_ is true if systrace is enabled or
  // measure_read_barrier_time_ is true.
  bool mark_from_read_barrier_measurements_;
  Atomic<uint64_t> rb_slow_path_ns_;
  Atomic<uint64_t> rb_slow_path_count_;
  Atomic<uint64_t> rb_slow_path_count_gc_;
  mutable Mutex rb_slow_path_histogram_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  Histogram<uint64_t> rb_slow_path_time_histogram_ GUARDED_BY(rb_slow_path_histogram_lock_);
  uint64_t rb_slow_path_count_total_ GUARDED_BY(rb_slow_path_histogram_lock_);
  uint64_t rb_slow_path_count_gc_total_ GUARDED_BY(rb_slow_path_histogram_lock_);

  accounting::ReadBarrierTable* rb_table_;
  bool force_evacuate_all_;  // True if all regions are evacuated.
  Atomic<bool> updated_all_immune_objects_;
  bool gc_grays_immune_objects_;
  Mutex immune_gray_stack_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::vector<mirror::Object*> immune_gray_stack_ GUARDED_BY(immune_gray_stack_lock_);

  // Class of java.lang.Object. Filled in from WellKnownClasses in FlipCallback. Must
  // be filled in before flipping thread roots so that FillDummyObject can run. Not
  // ObjPtr since the GC may transition to suspended and runnable between phases.
  mirror::Class* java_lang_Object_;

  class AssertToSpaceInvariantFieldVisitor;
  class AssertToSpaceInvariantObjectVisitor;
  class AssertToSpaceInvariantRefsVisitor;
  class ClearBlackPtrsVisitor;
  class ComputeUnevacFromSpaceLiveRatioVisitor;
  class DisableMarkingCallback;
  class DisableMarkingCheckpoint;
  class DisableWeakRefAccessCallback;
  class FlipCallback;
  class GrayImmuneObjectVisitor;
  class ImmuneSpaceScanObjVisitor;
  class LostCopyVisitor;
  class RefFieldsVisitor;
  class RevokeThreadLocalMarkStackCheckpoint;
  class ScopedGcGraysImmuneObjects;
  class ThreadFlipVisitor;
  class VerifyGrayImmuneObjectsVisitor;
  class VerifyNoFromSpaceRefsFieldVisitor;
  class VerifyNoFromSpaceRefsObjectVisitor;
  class VerifyNoFromSpaceRefsVisitor;
  class VerifyNoMissingCardMarkVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ConcurrentCopying);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
