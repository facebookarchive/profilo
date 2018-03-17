/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_H_
#define ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_H_

#include <memory>

#include "atomic.h"
#include "barrier.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "garbage_collector.h"
#include "gc_root.h"
#include "gc/accounting/heap_bitmap.h"
#include "immune_spaces.h"
#include "object_callbacks.h"
#include "offsets.h"

namespace art {

namespace mirror {
class Class;
class Object;
class Reference;
}  // namespace mirror

class Thread;
enum VisitRootFlags : uint8_t;

namespace gc {

class Heap;

namespace accounting {
template<typename T> class AtomicStack;
typedef AtomicStack<mirror::Object> ObjectStack;
}  // namespace accounting

namespace collector {

class MarkSweep : public GarbageCollector {
 public:
  MarkSweep(Heap* heap, bool is_concurrent, const std::string& name_prefix = "");

  ~MarkSweep() {}

  virtual void RunPhases() OVERRIDE REQUIRES(!mark_stack_lock_);
  void InitializePhase();
  void MarkingPhase() REQUIRES(!mark_stack_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
  void PausePhase() REQUIRES(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void ReclaimPhase() REQUIRES(!mark_stack_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
  void FinishPhase();
  virtual void MarkReachableObjects()
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsConcurrent() const {
    return is_concurrent_;
  }

  virtual GcType GetGcType() const OVERRIDE {
    return kGcTypeFull;
  }

  virtual CollectorType GetCollectorType() const OVERRIDE {
    return is_concurrent_ ? kCollectorTypeCMS : kCollectorTypeMS;
  }

  // Initializes internal structures.
  void Init();

  // Find the default mark bitmap.
  void FindDefaultSpaceBitmap() SHARED_REQUIRES(Locks::mutator_lock_);

  // Marks all objects in the root set at the start of a garbage collection.
  void MarkRoots(Thread* self)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void MarkNonThreadRoots()
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  virtual void MarkConcurrentRoots(VisitRootFlags flags)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void MarkRootsCheckpoint(Thread* self, bool revoke_ros_alloc_thread_local_buffers_at_checkpoint)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Builds a mark stack and recursively mark until it empties.
  void RecursiveMark()
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Bind the live bits to the mark bits of bitmaps for spaces that are never collected, ie
  // the image. Mark that portion of the heap as immune.
  virtual void BindBitmaps() SHARED_REQUIRES(Locks::mutator_lock_);

  // Builds a mark stack with objects on dirty cards and recursively mark until it empties.
  void RecursiveMarkDirtyObjects(bool paused, uint8_t minimum_age)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Remarks the root set after completing the concurrent mark.
  void ReMarkRoots()
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void ProcessReferences(Thread* self)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Update and mark references from immune spaces.
  void UpdateAndMarkModUnion()
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Pre clean cards to reduce how much work is needed in the pause.
  void PreCleanCards()
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Sweeps unmarked objects to complete the garbage collection. Virtual as by default it sweeps
  // all allocation spaces. Partial and sticky GCs want to just sweep a subset of the heap.
  virtual void Sweep(bool swap_bitmaps)
      REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Sweeps unmarked objects to complete the garbage collection.
  void SweepLargeObjects(bool swap_bitmaps) REQUIRES(Locks::heap_bitmap_lock_);

  // Sweep only pointers within an array. WARNING: Trashes objects.
  void SweepArray(accounting::ObjectStack* allocation_stack_, bool swap_bitmaps)
      REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Blackens an object.
  void ScanObject(mirror::Object* obj)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // No thread safety analysis due to lambdas.
  template<typename MarkVisitor, typename ReferenceVisitor>
  void ScanObjectVisit(mirror::Object* obj,
                       const MarkVisitor& visitor,
                       const ReferenceVisitor& ref_visitor)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void SweepSystemWeaks(Thread* self)
      REQUIRES(!Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static mirror::Object* VerifySystemWeakIsLiveCallback(mirror::Object* obj, void* arg)
      SHARED_REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void VerifySystemWeaks()
      SHARED_REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  // Verify that an object is live, either in a live bitmap or in the allocation stack.
  void VerifyIsLive(const mirror::Object* obj)
      SHARED_REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  virtual bool IsMarkedHeapReference(mirror::HeapReference<mirror::Object>* ref) OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  virtual void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info) OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  virtual void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                          size_t count,
                          const RootInfo& info) OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Marks an object.
  virtual mirror::Object* MarkObject(mirror::Object* obj) OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void MarkObject(mirror::Object* obj, mirror::Object* holder, MemberOffset offset)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  virtual void MarkHeapReference(mirror::HeapReference<mirror::Object>* ref) OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  Barrier& GetBarrier() {
    return *gc_barrier_;
  }

  // Schedules an unmarked object for reference processing.
  void DelayReferenceReferent(mirror::Class* klass, mirror::Reference* reference)
      SHARED_REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

 protected:
  // Returns object if the object is marked in the heap bitmap, otherwise null.
  virtual mirror::Object* IsMarked(mirror::Object* object) OVERRIDE
      SHARED_REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void MarkObjectNonNull(mirror::Object* obj,
                         mirror::Object* holder = nullptr,
                         MemberOffset offset = MemberOffset(0))
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Marks an object atomically, safe to use from multiple threads.
  void MarkObjectNonNullParallel(mirror::Object* obj)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns true if we need to add obj to a mark stack.
  bool MarkObjectParallel(mirror::Object* obj) NO_THREAD_SAFETY_ANALYSIS;

  // Verify the roots of the heap and print out information related to any invalid roots.
  // Called in MarkObject, so may we may not hold the mutator lock.
  void VerifyRoots()
      NO_THREAD_SAFETY_ANALYSIS;

  // Expand mark stack to 2x its current size.
  void ExpandMarkStack()
      REQUIRES(mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void ResizeMarkStack(size_t new_size)
      REQUIRES(mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns how many threads we should use for the current GC phase based on if we are paused,
  // whether or not we care about pauses.
  size_t GetThreadCount(bool paused) const;

  // Push a single reference on a mark stack.
  void PushOnMarkStack(mirror::Object* obj)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Blackens objects grayed during a garbage collection.
  void ScanGrayObjects(bool paused, uint8_t minimum_age)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  virtual void ProcessMarkStack()
      OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    ProcessMarkStack(false);
  }

  // Recursively blackens objects on the mark stack.
  void ProcessMarkStack(bool paused)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void ProcessMarkStackParallel(size_t thread_count)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!mark_stack_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Used to Get around thread safety annotations. The call is from MarkingPhase and is guarded by
  // IsExclusiveHeld.
  void RevokeAllThreadLocalAllocationStacks(Thread* self) NO_THREAD_SAFETY_ANALYSIS;

  // Revoke all the thread-local buffers.
  void RevokeAllThreadLocalBuffers();

  // Whether or not we count how many of each type of object were scanned.
  static constexpr bool kCountScannedTypes = false;

  // Current space, we check this space first to avoid searching for the appropriate space for an
  // object.
  accounting::ContinuousSpaceBitmap* current_space_bitmap_;
  // Cache the heap's mark bitmap to prevent having to do 2 loads during slow path marking.
  accounting::HeapBitmap* mark_bitmap_;

  accounting::ObjectStack* mark_stack_;

  // Every object inside the immune spaces is assumed to be marked. Immune spaces that aren't in the
  // immune region are handled by the normal marking logic.
  ImmuneSpaces immune_spaces_;

  // Parallel finger.
  AtomicInteger atomic_finger_;

  AtomicInteger no_reference_class_count_;
  AtomicInteger normal_count_;
  // Number of classes scanned, if kCountScannedTypes.
  AtomicInteger class_count_;
  // Number of object arrays scanned, if kCountScannedTypes.
  AtomicInteger object_array_count_;
  // Number of non-class/arrays scanned, if kCountScannedTypes.
  AtomicInteger other_count_;
  // Number of java.lang.ref.Reference instances.
  AtomicInteger reference_count_;

  AtomicInteger large_object_test_;
  AtomicInteger large_object_mark_;
  AtomicInteger overhead_time_;
  AtomicInteger work_chunks_created_;
  AtomicInteger work_chunks_deleted_;
  AtomicInteger mark_null_count_;
  AtomicInteger mark_immune_count_;
  AtomicInteger mark_fastpath_count_;
  AtomicInteger mark_slowpath_count_;

  std::unique_ptr<Barrier> gc_barrier_;
  Mutex mark_stack_lock_ ACQUIRED_AFTER(Locks::classlinker_classes_lock_);

  const bool is_concurrent_;

  // Verification.
  size_t live_stack_freeze_size_;

  std::unique_ptr<MemMap> sweep_array_free_buffer_mem_map_;

 private:
  class CardScanTask;
  class CheckpointMarkThreadRoots;
  class DelayReferenceReferentVisitor;
  template<bool kUseFinger> class MarkStackTask;
  class MarkObjectSlowPath;
  class RecursiveMarkTask;
  class ScanObjectParallelVisitor;
  class ScanObjectVisitor;
  class VerifyRootMarkedVisitor;
  class VerifyRootVisitor;
  class VerifySystemWeakVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MarkSweep);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_H_
