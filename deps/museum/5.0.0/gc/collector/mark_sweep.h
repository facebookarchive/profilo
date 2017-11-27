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
#include "gc/accounting/heap_bitmap.h"
#include "immune_region.h"
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
  typedef AtomicStack<mirror::Object*> ObjectStack;
}  // namespace accounting

namespace collector {

class MarkSweep : public GarbageCollector {
 public:
  explicit MarkSweep(Heap* heap, bool is_concurrent, const std::string& name_prefix = "");

  ~MarkSweep() {}

  virtual void RunPhases() OVERRIDE NO_THREAD_SAFETY_ANALYSIS;
  void InitializePhase();
  void MarkingPhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void PausePhase() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ReclaimPhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void FinishPhase() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual void MarkReachableObjects()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

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
  void FindDefaultSpaceBitmap();

  // Marks all objects in the root set at the start of a garbage collection.
  void MarkRoots(Thread* self)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void MarkNonThreadRoots()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void MarkConcurrentRoots(VisitRootFlags flags)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void MarkRootsCheckpoint(Thread* self, bool revoke_ros_alloc_thread_local_buffers_at_checkpoint)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Builds a mark stack and recursively mark until it empties.
  void RecursiveMark()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Bind the live bits to the mark bits of bitmaps for spaces that are never collected, ie
  // the image. Mark that portion of the heap as immune.
  virtual void BindBitmaps() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Builds a mark stack with objects on dirty cards and recursively mark until it empties.
  void RecursiveMarkDirtyObjects(bool paused, byte minimum_age)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Remarks the root set after completing the concurrent mark.
  void ReMarkRoots()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ProcessReferences(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Update and mark references from immune spaces.
  void UpdateAndMarkModUnion()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Pre clean cards to reduce how much work is needed in the pause.
  void PreCleanCards()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Sweeps unmarked objects to complete the garbage collection. Virtual as by default it sweeps
  // all allocation spaces. Partial and sticky GCs want to just sweep a subset of the heap.
  virtual void Sweep(bool swap_bitmaps) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Sweeps unmarked objects to complete the garbage collection.
  void SweepLargeObjects(bool swap_bitmaps) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Sweep only pointers within an array. WARNING: Trashes objects.
  void SweepArray(accounting::ObjectStack* allocation_stack_, bool swap_bitmaps)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Blackens an object.
  void ScanObject(mirror::Object* obj)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // No thread safety analysis due to lambdas.
  template<typename MarkVisitor, typename ReferenceVisitor>
  void ScanObjectVisit(mirror::Object* obj, const MarkVisitor& visitor,
                       const ReferenceVisitor& ref_visitor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
    EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  void SweepSystemWeaks(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(Locks::heap_bitmap_lock_);

  static mirror::Object* VerifySystemWeakIsLiveCallback(mirror::Object* obj, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  void VerifySystemWeaks()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  // Verify that an object is live, either in a live bitmap or in the allocation stack.
  void VerifyIsLive(const mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static mirror::Object* MarkObjectCallback(mirror::Object* obj, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static void MarkHeapReferenceCallback(mirror::HeapReference<mirror::Object>* ref, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static bool HeapReferenceMarkedCallback(mirror::HeapReference<mirror::Object>* ref, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static void MarkRootCallback(mirror::Object** root, void* arg, uint32_t thread_id,
                               RootType root_type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static void VerifyRootMarked(mirror::Object** root, void* arg, uint32_t /*thread_id*/,
                               RootType /*root_type*/)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static void ProcessMarkStackCallback(void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void MarkRootParallelCallback(mirror::Object** root, void* arg, uint32_t thread_id,
                                       RootType root_type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Marks an object.
  void MarkObject(mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  Barrier& GetBarrier() {
    return *gc_barrier_;
  }

  // Schedules an unmarked object for reference processing.
  void DelayReferenceReferent(mirror::Class* klass, mirror::Reference* reference)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

 protected:
  // Returns true if the object has its bit set in the mark bitmap.
  bool IsMarked(const mirror::Object* object) const
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static mirror::Object* IsMarkedCallback(mirror::Object* object, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static void VerifyImageRootVisitor(mirror::Object* root, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void MarkObjectNonNull(mirror::Object* obj)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Marks an object atomically, safe to use from multiple threads.
  void MarkObjectNonNullParallel(mirror::Object* obj);

  // Returns true if we need to add obj to a mark stack.
  bool MarkObjectParallel(const mirror::Object* obj) NO_THREAD_SAFETY_ANALYSIS;

  // Verify the roots of the heap and print out information related to any invalid roots.
  // Called in MarkObject, so may we may not hold the mutator lock.
  void VerifyRoots()
      NO_THREAD_SAFETY_ANALYSIS;

  // Expand mark stack to 2x its current size.
  void ExpandMarkStack() EXCLUSIVE_LOCKS_REQUIRED(mark_stack_lock_);
  void ResizeMarkStack(size_t new_size) EXCLUSIVE_LOCKS_REQUIRED(mark_stack_lock_);

  // Returns how many threads we should use for the current GC phase based on if we are paused,
  // whether or not we care about pauses.
  size_t GetThreadCount(bool paused) const;

  static void VerifyRootCallback(const mirror::Object* root, void* arg, size_t vreg,
                                 const StackVisitor *visitor, RootType root_type);

  void VerifyRoot(const mirror::Object* root, size_t vreg, const StackVisitor* visitor,
                  RootType root_type) NO_THREAD_SAFETY_ANALYSIS;

  // Push a single reference on a mark stack.
  void PushOnMarkStack(mirror::Object* obj);

  // Blackens objects grayed during a garbage collection.
  void ScanGrayObjects(bool paused, byte minimum_age)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Recursively blackens objects on the mark stack.
  void ProcessMarkStack(bool paused)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ProcessMarkStackParallel(size_t thread_count)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Used to Get around thread safety annotations. The call is from MarkingPhase and is guarded by
  // IsExclusiveHeld.
  void RevokeAllThreadLocalAllocationStacks(Thread* self) NO_THREAD_SAFETY_ANALYSIS;

  // Revoke all the thread-local buffers.
  void RevokeAllThreadLocalBuffers();

  // Whether or not we count how many of each type of object were scanned.
  static const bool kCountScannedTypes = false;

  // Current space, we check this space first to avoid searching for the appropriate space for an
  // object.
  accounting::ContinuousSpaceBitmap* current_space_bitmap_;
  // Cache the heap's mark bitmap to prevent having to do 2 loads during slow path marking.
  accounting::HeapBitmap* mark_bitmap_;

  accounting::ObjectStack* mark_stack_;

  // Immune region, every object inside the immune range is assumed to be marked.
  ImmuneRegion immune_region_;

  // Parallel finger.
  AtomicInteger atomic_finger_;
  // Number of classes scanned, if kCountScannedTypes.
  AtomicInteger class_count_;
  // Number of arrays scanned, if kCountScannedTypes.
  AtomicInteger array_count_;
  // Number of non-class/arrays scanned, if kCountScannedTypes.
  AtomicInteger other_count_;
  AtomicInteger large_object_test_;
  AtomicInteger large_object_mark_;
  AtomicInteger overhead_time_;
  AtomicInteger work_chunks_created_;
  AtomicInteger work_chunks_deleted_;
  AtomicInteger reference_count_;
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
  friend class AddIfReachesAllocSpaceVisitor;  // Used by mod-union table.
  friend class CardScanTask;
  friend class CheckBitmapVisitor;
  friend class CheckReferenceVisitor;
  friend class art::gc::Heap;
  friend class MarkObjectVisitor;
  friend class ModUnionCheckReferences;
  friend class ModUnionClearCardVisitor;
  friend class ModUnionReferenceVisitor;
  friend class ModUnionVisitor;
  friend class ModUnionTableBitmap;
  friend class ModUnionTableReferenceCache;
  friend class ModUnionScanImageRootVisitor;
  template<bool kUseFinger> friend class MarkStackTask;
  friend class FifoMarkStackChunk;
  friend class MarkSweepMarkObjectSlowPath;

  DISALLOW_COPY_AND_ASSIGN(MarkSweep);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_H_
