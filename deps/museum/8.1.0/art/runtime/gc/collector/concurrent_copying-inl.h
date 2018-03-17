/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_
#define ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_

#include "concurrent_copying.h"

#include "gc/accounting/atomic_stack.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/heap.h"
#include "gc/space/region_space.h"
#include "lock_word.h"
#include "mirror/object-readbarrier-inl.h"

namespace art {
namespace gc {
namespace collector {

inline mirror::Object* ConcurrentCopying::MarkUnevacFromSpaceRegion(
    mirror::Object* ref, accounting::ContinuousSpaceBitmap* bitmap) {
  // For the Baker-style RB, in a rare case, we could incorrectly change the object from white
  // to gray even though the object has already been marked through. This happens if a mutator
  // thread gets preempted before the AtomicSetReadBarrierState below, GC marks through the
  // object (changes it from white to gray and back to white), and the thread runs and
  // incorrectly changes it from white to gray. If this happens, the object will get added to the
  // mark stack again and get changed back to white after it is processed.
  if (kUseBakerReadBarrier) {
    // Test the bitmap first to avoid graying an object that has already been marked through most
    // of the time.
    if (bitmap->Test(ref)) {
      return ref;
    }
  }
  // This may or may not succeed, which is ok because the object may already be gray.
  bool success = false;
  if (kUseBakerReadBarrier) {
    // GC will mark the bitmap when popping from mark stack. If only the GC is touching the bitmap
    // we can avoid an expensive CAS.
    // For the baker case, an object is marked if either the mark bit marked or the bitmap bit is
    // set.
    success = ref->AtomicSetReadBarrierState(ReadBarrier::WhiteState(), ReadBarrier::GrayState());
  } else {
    success = !bitmap->AtomicTestAndSet(ref);
  }
  if (success) {
    // Newly marked.
    if (kUseBakerReadBarrier) {
      DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::GrayState());
    }
    PushOntoMarkStack(ref);
  }
  return ref;
}

template<bool kGrayImmuneObject>
inline mirror::Object* ConcurrentCopying::MarkImmuneSpace(mirror::Object* ref) {
  if (kUseBakerReadBarrier) {
    // The GC-running thread doesn't (need to) gray immune objects except when updating thread roots
    // in the thread flip on behalf of suspended threads (when gc_grays_immune_objects_ is
    // true). Also, a mutator doesn't (need to) gray an immune object after GC has updated all
    // immune space objects (when updated_all_immune_objects_ is true).
    if (kIsDebugBuild) {
      if (Thread::Current() == thread_running_gc_) {
        DCHECK(!kGrayImmuneObject ||
               updated_all_immune_objects_.LoadRelaxed() ||
               gc_grays_immune_objects_);
      } else {
        DCHECK(kGrayImmuneObject);
      }
    }
    if (!kGrayImmuneObject || updated_all_immune_objects_.LoadRelaxed()) {
      return ref;
    }
    // This may or may not succeed, which is ok because the object may already be gray.
    bool success = ref->AtomicSetReadBarrierState(ReadBarrier::WhiteState(),
                                                  ReadBarrier::GrayState());
    if (success) {
      MutexLock mu(Thread::Current(), immune_gray_stack_lock_);
      immune_gray_stack_.push_back(ref);
    }
  }
  return ref;
}

template<bool kGrayImmuneObject, bool kFromGCThread>
inline mirror::Object* ConcurrentCopying::Mark(mirror::Object* from_ref,
                                               mirror::Object* holder,
                                               MemberOffset offset) {
  if (from_ref == nullptr) {
    return nullptr;
  }
  DCHECK(heap_->collector_type_ == kCollectorTypeCC);
  if (kFromGCThread) {
    DCHECK(is_active_);
    DCHECK_EQ(Thread::Current(), thread_running_gc_);
  } else if (UNLIKELY(kUseBakerReadBarrier && !is_active_)) {
    // In the lock word forward address state, the read barrier bits
    // in the lock word are part of the stored forwarding address and
    // invalid. This is usually OK as the from-space copy of objects
    // aren't accessed by mutators due to the to-space
    // invariant. However, during the dex2oat image writing relocation
    // and the zygote compaction, objects can be in the forward
    // address state (to store the forward/relocation addresses) and
    // they can still be accessed and the invalid read barrier bits
    // are consulted. If they look like gray but aren't really, the
    // read barriers slow path can trigger when it shouldn't. To guard
    // against this, return here if the CC collector isn't running.
    return from_ref;
  }
  DCHECK(region_space_ != nullptr) << "Read barrier slow path taken when CC isn't running?";
  space::RegionSpace::RegionType rtype = region_space_->GetRegionType(from_ref);
  switch (rtype) {
    case space::RegionSpace::RegionType::kRegionTypeToSpace:
      // It's already marked.
      return from_ref;
    case space::RegionSpace::RegionType::kRegionTypeFromSpace: {
      mirror::Object* to_ref = GetFwdPtr(from_ref);
      if (to_ref == nullptr) {
        // It isn't marked yet. Mark it by copying it to the to-space.
        to_ref = Copy(from_ref, holder, offset);
      }
      DCHECK(region_space_->IsInToSpace(to_ref) || heap_->non_moving_space_->HasAddress(to_ref))
          << "from_ref=" << from_ref << " to_ref=" << to_ref;
      return to_ref;
    }
    case space::RegionSpace::RegionType::kRegionTypeUnevacFromSpace: {
      return MarkUnevacFromSpaceRegion(from_ref, region_space_bitmap_);
    }
    case space::RegionSpace::RegionType::kRegionTypeNone:
      if (immune_spaces_.ContainsObject(from_ref)) {
        return MarkImmuneSpace<kGrayImmuneObject>(from_ref);
      } else {
        return MarkNonMoving(from_ref, holder, offset);
      }
    default:
      UNREACHABLE();
  }
}

inline mirror::Object* ConcurrentCopying::MarkFromReadBarrier(mirror::Object* from_ref) {
  mirror::Object* ret;
  // We can get here before marking starts since we gray immune objects before the marking phase.
  if (from_ref == nullptr || !Thread::Current()->GetIsGcMarking()) {
    return from_ref;
  }
  // TODO: Consider removing this check when we are done investigating slow paths. b/30162165
  if (UNLIKELY(mark_from_read_barrier_measurements_)) {
    ret = MarkFromReadBarrierWithMeasurements(from_ref);
  } else {
    ret = Mark(from_ref);
  }
  // Only set the mark bit for baker barrier.
  if (kUseBakerReadBarrier && LIKELY(!rb_mark_bit_stack_full_ && ret->AtomicSetMarkBit(0, 1))) {
    // If the mark stack is full, we may temporarily go to mark and back to unmarked. Seeing both
    // values are OK since the only race is doing an unnecessary Mark.
    if (!rb_mark_bit_stack_->AtomicPushBack(ret)) {
      // Mark stack is full, set the bit back to zero.
      CHECK(ret->AtomicSetMarkBit(1, 0));
      // Set rb_mark_bit_stack_full_, this is racy but OK since AtomicPushBack is thread safe.
      rb_mark_bit_stack_full_ = true;
    }
  }
  return ret;
}

inline mirror::Object* ConcurrentCopying::GetFwdPtr(mirror::Object* from_ref) {
  DCHECK(region_space_->IsInFromSpace(from_ref));
  LockWord lw = from_ref->GetLockWord(false);
  if (lw.GetState() == LockWord::kForwardingAddress) {
    mirror::Object* fwd_ptr = reinterpret_cast<mirror::Object*>(lw.ForwardingAddress());
    DCHECK(fwd_ptr != nullptr);
    return fwd_ptr;
  } else {
    return nullptr;
  }
}

inline bool ConcurrentCopying::IsMarkedInUnevacFromSpace(mirror::Object* from_ref) {
  // Use load acquire on the read barrier pointer to ensure that we never see a white read barrier
  // state with an unmarked bit due to reordering.
  DCHECK(region_space_->IsInUnevacFromSpace(from_ref));
  if (kUseBakerReadBarrier && from_ref->GetReadBarrierStateAcquire() == ReadBarrier::GrayState()) {
    return true;
  }
  return region_space_bitmap_->Test(from_ref);
}

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_
