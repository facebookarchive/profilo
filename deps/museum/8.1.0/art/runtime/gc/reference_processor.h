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

#ifndef ART_RUNTIME_GC_REFERENCE_PROCESSOR_H_
#define ART_RUNTIME_GC_REFERENCE_PROCESSOR_H_

#include "base/mutex.h"
#include "globals.h"
#include "jni.h"
#include "reference_queue.h"

namespace art {

class IsMarkedVisitor;
class TimingLogger;

namespace mirror {
class Class;
class FinalizerReference;
class Object;
class Reference;
}  // namespace mirror

namespace gc {

namespace collector {
class GarbageCollector;
}  // namespace collector

class Heap;

// Used to process java.lang.ref.Reference instances concurrently or paused.
class ReferenceProcessor {
 public:
  explicit ReferenceProcessor();
  void ProcessReferences(bool concurrent,
                         TimingLogger* timings,
                         bool clear_soft_references,
                         gc::collector::GarbageCollector* collector)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES(!Locks::reference_processor_lock_);
  // The slow path bool is contained in the reference class object, can only be set once
  // Only allow setting this with mutators suspended so that we can avoid using a lock in the
  // GetReferent fast path as an optimization.
  void EnableSlowPath() REQUIRES_SHARED(Locks::mutator_lock_);
  void BroadcastForSlowPath(Thread* self);
  // Decode the referent, may block if references are being processed.
  ObjPtr<mirror::Object> GetReferent(Thread* self, ObjPtr<mirror::Reference> reference)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::reference_processor_lock_);
  void EnqueueClearedReferences(Thread* self) REQUIRES(!Locks::mutator_lock_);
  void DelayReferenceReferent(ObjPtr<mirror::Class> klass,
                              ObjPtr<mirror::Reference> ref,
                              collector::GarbageCollector* collector)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void UpdateRoots(IsMarkedVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  // Make a circular list with reference if it is not enqueued. Uses the finalizer queue lock.
  bool MakeCircularListIfUnenqueued(ObjPtr<mirror::FinalizerReference> reference)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::reference_processor_lock_,
               !Locks::reference_queue_finalizer_references_lock_);
  void ClearReferent(ObjPtr<mirror::Reference> ref)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::reference_processor_lock_);

 private:
  bool SlowPathEnabled() REQUIRES_SHARED(Locks::mutator_lock_);
  // Called by ProcessReferences.
  void DisableSlowPath(Thread* self) REQUIRES(Locks::reference_processor_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // If we are preserving references it means that some dead objects may become live, we use start
  // and stop preserving to block mutators using GetReferrent from getting access to these
  // referents.
  void StartPreservingReferences(Thread* self) REQUIRES(!Locks::reference_processor_lock_);
  void StopPreservingReferences(Thread* self) REQUIRES(!Locks::reference_processor_lock_);
  // Wait until reference processing is done.
  void WaitUntilDoneProcessingReferences(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(Locks::reference_processor_lock_);
  // Collector which is clearing references, used by the GetReferent to return referents which are
  // already marked.
  collector::GarbageCollector* collector_ GUARDED_BY(Locks::reference_processor_lock_);
  // Boolean for whether or not we are preserving references (either soft references or finalizers).
  // If this is true, then we cannot return a referent (see comment in GetReferent).
  bool preserving_references_ GUARDED_BY(Locks::reference_processor_lock_);
  // Condition that people wait on if they attempt to get the referent of a reference while
  // processing is in progress.
  ConditionVariable condition_ GUARDED_BY(Locks::reference_processor_lock_);
  // Reference queues used by the GC.
  ReferenceQueue soft_reference_queue_;
  ReferenceQueue weak_reference_queue_;
  ReferenceQueue finalizer_reference_queue_;
  ReferenceQueue phantom_reference_queue_;
  ReferenceQueue cleared_references_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceProcessor);
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_REFERENCE_PROCESSOR_H_
