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

#ifndef ART_RUNTIME_GC_HEAP_VISIT_OBJECTS_INL_H_
#define ART_RUNTIME_GC_HEAP_VISIT_OBJECTS_INL_H_

#include "heap.h"

#include "base/mutex-inl.h"
#include "gc/accounting/heap_bitmap-inl.h"
#include "gc/space/bump_pointer_space-walk-inl.h"
#include "gc/space/region_space-inl.h"
#include "mirror/object-inl.h"
#include "obj_ptr-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"

namespace art {
namespace gc {

// Visit objects when threads aren't suspended. If concurrent moving
// GC, disable moving GC and suspend threads and then visit objects.
template <typename Visitor>
inline void Heap::VisitObjects(Visitor&& visitor) {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertSharedHeld(self);
  DCHECK(!Locks::mutator_lock_->IsExclusiveHeld(self)) << "Call VisitObjectsPaused() instead";
  if (IsGcConcurrentAndMoving()) {
    // Concurrent moving GC. Just suspending threads isn't sufficient
    // because a collection isn't one big pause and we could suspend
    // threads in the middle (between phases) of a concurrent moving
    // collection where it's not easily known which objects are alive
    // (both the region space and the non-moving space) or which
    // copies of objects to visit, and the to-space invariant could be
    // easily broken. Visit objects while GC isn't running by using
    // IncrementDisableMovingGC() and threads are suspended.
    IncrementDisableMovingGC(self);
    {
      ScopedThreadSuspension sts(self, kWaitingForVisitObjects);
      ScopedSuspendAll ssa(__FUNCTION__);
      VisitObjectsInternalRegionSpace(visitor);
      VisitObjectsInternal(visitor);
    }
    DecrementDisableMovingGC(self);
  } else {
    // Since concurrent moving GC has thread suspension, also poison ObjPtr the normal case to
    // catch bugs.
    self->PoisonObjectPointers();
    // GCs can move objects, so don't allow this.
    ScopedAssertNoThreadSuspension ants("Visiting objects");
    DCHECK(region_space_ == nullptr);
    VisitObjectsInternal(visitor);
    self->PoisonObjectPointers();
  }
}

template <typename Visitor>
inline void Heap::VisitObjectsPaused(Visitor&& visitor) {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  VisitObjectsInternalRegionSpace(visitor);
  VisitObjectsInternal(visitor);
}

// Visit objects in the region spaces.
template <typename Visitor>
inline void Heap::VisitObjectsInternalRegionSpace(Visitor&& visitor) {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  if (region_space_ != nullptr) {
    DCHECK(IsGcConcurrentAndMoving());
    if (!zygote_creation_lock_.IsExclusiveHeld(self)) {
      // Exclude the pre-zygote fork time where the semi-space collector
      // calls VerifyHeapReferences() as part of the zygote compaction
      // which then would call here without the moving GC disabled,
      // which is fine.
      bool is_thread_running_gc = false;
      if (kIsDebugBuild) {
        MutexLock mu(self, *gc_complete_lock_);
        is_thread_running_gc = self == thread_running_gc_;
      }
      // If we are not the thread running the GC on in a GC exclusive region, then moving GC
      // must be disabled.
      DCHECK(is_thread_running_gc || IsMovingGCDisabled(self));
    }
    region_space_->Walk(visitor);
  }
}

// Visit objects in the other spaces.
template <typename Visitor>
inline void Heap::VisitObjectsInternal(Visitor&& visitor) {
  if (bump_pointer_space_ != nullptr) {
    // Visit objects in bump pointer space.
    bump_pointer_space_->Walk(visitor);
  }
  // TODO: Switch to standard begin and end to use ranged a based loop.
  for (auto* it = allocation_stack_->Begin(), *end = allocation_stack_->End(); it < end; ++it) {
    mirror::Object* const obj = it->AsMirrorPtr();

    mirror::Class* kls = nullptr;
    if (obj != nullptr && (kls = obj->GetClass()) != nullptr) {
      // Below invariant is safe regardless of what space the Object is in.
      // For speed reasons, only perform it when Rosalloc could possibly be used.
      // (Disabled for read barriers because it never uses Rosalloc).
      // (See the DCHECK in RosAllocSpace constructor).
      if (!kUseReadBarrier) {
        // Rosalloc has a race in allocation. Objects can be written into the allocation
        // stack before their header writes are visible to this thread.
        // See b/28790624 for more details.
        //
        // obj.class will either be pointing to a valid Class*, or it will point
        // to a rosalloc free buffer.
        //
        // If it's pointing to a valid Class* then that Class's Class will be the
        // ClassClass (whose Class is itself).
        //
        // A rosalloc free buffer will point to another rosalloc free buffer
        // (or to null), and never to itself.
        //
        // Either way dereferencing while its not-null is safe because it will
        // always point to another valid pointer or to null.
        mirror::Class* klsClass = kls->GetClass();

        if (klsClass == nullptr) {
          continue;
        } else if (klsClass->GetClass() != klsClass) {
          continue;
        }
      } else {
        // Ensure the invariant is not broken for non-rosalloc cases.
        DCHECK(Heap::rosalloc_space_ == nullptr)
            << "unexpected rosalloc with read barriers";
        DCHECK(kls->GetClass() != nullptr)
            << "invalid object: class does not have a class";
        DCHECK_EQ(kls->GetClass()->GetClass(), kls->GetClass())
            << "invalid object: class's class is not ClassClass";
      }

      // Avoid the race condition caused by the object not yet being written into the allocation
      // stack or the class not yet being written in the object. Or, if
      // kUseThreadLocalAllocationStack, there can be nulls on the allocation stack.
      visitor(obj);
    }
  }
  {
    ReaderMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    GetLiveBitmap()->Visit<Visitor>(visitor);
  }
}

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_HEAP_VISIT_OBJECTS_INL_H_
