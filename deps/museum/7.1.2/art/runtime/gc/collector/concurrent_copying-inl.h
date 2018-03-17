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

#include "gc/accounting/space_bitmap-inl.h"
#include "gc/heap.h"
#include "gc/space/region_space.h"
#include "lock_word.h"

namespace art {
namespace gc {
namespace collector {

inline mirror::Object* ConcurrentCopying::Mark(mirror::Object* from_ref) {
  if (from_ref == nullptr) {
    return nullptr;
  }
  DCHECK(heap_->collector_type_ == kCollectorTypeCC);
  if (UNLIKELY(kUseBakerReadBarrier && !is_active_)) {
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
      if (kUseBakerReadBarrier) {
        DCHECK_NE(to_ref, ReadBarrier::GrayPtr())
            << "from_ref=" << from_ref << " to_ref=" << to_ref;
      }
      if (to_ref == nullptr) {
        // It isn't marked yet. Mark it by copying it to the to-space.
        to_ref = Copy(from_ref);
      }
      DCHECK(region_space_->IsInToSpace(to_ref) || heap_->non_moving_space_->HasAddress(to_ref))
          << "from_ref=" << from_ref << " to_ref=" << to_ref;
      return to_ref;
    }
    case space::RegionSpace::RegionType::kRegionTypeUnevacFromSpace: {
      // This may or may not succeed, which is ok.
      if (kUseBakerReadBarrier) {
        from_ref->AtomicSetReadBarrierPointer(ReadBarrier::WhitePtr(), ReadBarrier::GrayPtr());
      }
      mirror::Object* to_ref = from_ref;
      if (region_space_bitmap_->AtomicTestAndSet(from_ref)) {
        // Already marked.
      } else {
        // Newly marked.
        if (kUseBakerReadBarrier) {
          DCHECK_EQ(to_ref->GetReadBarrierPointer(), ReadBarrier::GrayPtr());
        }
        PushOntoMarkStack(to_ref);
      }
      return to_ref;
    }
    case space::RegionSpace::RegionType::kRegionTypeNone:
      return MarkNonMoving(from_ref);
    default:
      UNREACHABLE();
  }
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

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_INL_H_
