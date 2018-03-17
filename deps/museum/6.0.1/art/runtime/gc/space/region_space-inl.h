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

#ifndef ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_

#include "region_space.h"

namespace art {
namespace gc {
namespace space {

inline mirror::Object* RegionSpace::Alloc(Thread*, size_t num_bytes, size_t* bytes_allocated,
                                          size_t* usable_size,
                                          size_t* bytes_tl_bulk_allocated) {
  num_bytes = RoundUp(num_bytes, kAlignment);
  return AllocNonvirtual<false>(num_bytes, bytes_allocated, usable_size,
                                bytes_tl_bulk_allocated);
}

inline mirror::Object* RegionSpace::AllocThreadUnsafe(Thread* self, size_t num_bytes,
                                                      size_t* bytes_allocated,
                                                      size_t* usable_size,
                                                      size_t* bytes_tl_bulk_allocated) {
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  return Alloc(self, num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
}

template<bool kForEvac>
inline mirror::Object* RegionSpace::AllocNonvirtual(size_t num_bytes, size_t* bytes_allocated,
                                                    size_t* usable_size,
                                                    size_t* bytes_tl_bulk_allocated) {
  DCHECK(IsAligned<kAlignment>(num_bytes));
  mirror::Object* obj;
  if (LIKELY(num_bytes <= kRegionSize)) {
    // Non-large object.
    if (!kForEvac) {
      obj = current_region_->Alloc(num_bytes, bytes_allocated, usable_size,
                                   bytes_tl_bulk_allocated);
    } else {
      DCHECK(evac_region_ != nullptr);
      obj = evac_region_->Alloc(num_bytes, bytes_allocated, usable_size,
                                bytes_tl_bulk_allocated);
    }
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    MutexLock mu(Thread::Current(), region_lock_);
    // Retry with current region since another thread may have updated it.
    if (!kForEvac) {
      obj = current_region_->Alloc(num_bytes, bytes_allocated, usable_size,
                                   bytes_tl_bulk_allocated);
    } else {
      obj = evac_region_->Alloc(num_bytes, bytes_allocated, usable_size,
                                bytes_tl_bulk_allocated);
    }
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    if (!kForEvac) {
      // Retain sufficient free regions for full evacuation.
      if ((num_non_free_regions_ + 1) * 2 > num_regions_) {
        return nullptr;
      }
      for (size_t i = 0; i < num_regions_; ++i) {
        Region* r = &regions_[i];
        if (r->IsFree()) {
          r->Unfree(time_);
          r->SetNewlyAllocated();
          ++num_non_free_regions_;
          obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
          CHECK(obj != nullptr);
          current_region_ = r;
          return obj;
        }
      }
    } else {
      for (size_t i = 0; i < num_regions_; ++i) {
        Region* r = &regions_[i];
        if (r->IsFree()) {
          r->Unfree(time_);
          ++num_non_free_regions_;
          obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
          CHECK(obj != nullptr);
          evac_region_ = r;
          return obj;
        }
      }
    }
  } else {
    // Large object.
    obj = AllocLarge<kForEvac>(num_bytes, bytes_allocated, usable_size,
                               bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
  }
  return nullptr;
}

inline mirror::Object* RegionSpace::Region::Alloc(size_t num_bytes, size_t* bytes_allocated,
                                                  size_t* usable_size,
                                                  size_t* bytes_tl_bulk_allocated) {
  DCHECK(IsAllocated() && IsInToSpace());
  DCHECK(IsAligned<kAlignment>(num_bytes));
  Atomic<uint8_t*>* atomic_top = reinterpret_cast<Atomic<uint8_t*>*>(&top_);
  uint8_t* old_top;
  uint8_t* new_top;
  do {
    old_top = atomic_top->LoadRelaxed();
    new_top = old_top + num_bytes;
    if (UNLIKELY(new_top > end_)) {
      return nullptr;
    }
  } while (!atomic_top->CompareExchangeWeakSequentiallyConsistent(old_top, new_top));
  reinterpret_cast<Atomic<uint64_t>*>(&objects_allocated_)->FetchAndAddSequentiallyConsistent(1);
  DCHECK_LE(atomic_top->LoadRelaxed(), end_);
  DCHECK_LT(old_top, end_);
  DCHECK_LE(new_top, end_);
  *bytes_allocated = num_bytes;
  if (usable_size != nullptr) {
    *usable_size = num_bytes;
  }
  *bytes_tl_bulk_allocated = num_bytes;
  return reinterpret_cast<mirror::Object*>(old_top);
}

inline size_t RegionSpace::AllocationSizeNonvirtual(mirror::Object* obj, size_t* usable_size)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  size_t num_bytes = obj->SizeOf();
  if (usable_size != nullptr) {
    if (LIKELY(num_bytes <= kRegionSize)) {
      DCHECK(RefToRegion(obj)->IsAllocated());
      *usable_size = RoundUp(num_bytes, kAlignment);
    } else {
      DCHECK(RefToRegion(obj)->IsLarge());
      *usable_size = RoundUp(num_bytes, kRegionSize);
    }
  }
  return num_bytes;
}

template<RegionSpace::RegionType kRegionType>
uint64_t RegionSpace::GetBytesAllocatedInternal() {
  uint64_t bytes = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree()) {
      continue;
    }
    switch (kRegionType) {
      case RegionType::kRegionTypeAll:
        bytes += r->BytesAllocated();
        break;
      case RegionType::kRegionTypeFromSpace:
        if (r->IsInFromSpace()) {
          bytes += r->BytesAllocated();
        }
        break;
      case RegionType::kRegionTypeUnevacFromSpace:
        if (r->IsInUnevacFromSpace()) {
          bytes += r->BytesAllocated();
        }
        break;
      case RegionType::kRegionTypeToSpace:
        if (r->IsInToSpace()) {
          bytes += r->BytesAllocated();
        }
        break;
      default:
        LOG(FATAL) << "Unexpected space type : " << kRegionType;
    }
  }
  return bytes;
}

template<RegionSpace::RegionType kRegionType>
uint64_t RegionSpace::GetObjectsAllocatedInternal() {
  uint64_t bytes = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree()) {
      continue;
    }
    switch (kRegionType) {
      case RegionType::kRegionTypeAll:
        bytes += r->ObjectsAllocated();
        break;
      case RegionType::kRegionTypeFromSpace:
        if (r->IsInFromSpace()) {
          bytes += r->ObjectsAllocated();
        }
        break;
      case RegionType::kRegionTypeUnevacFromSpace:
        if (r->IsInUnevacFromSpace()) {
          bytes += r->ObjectsAllocated();
        }
        break;
      case RegionType::kRegionTypeToSpace:
        if (r->IsInToSpace()) {
          bytes += r->ObjectsAllocated();
        }
        break;
      default:
        LOG(FATAL) << "Unexpected space type : " << kRegionType;
    }
  }
  return bytes;
}

template<bool kToSpaceOnly>
void RegionSpace::WalkInternal(ObjectCallback* callback, void* arg) {
  // TODO: MutexLock on region_lock_ won't work due to lock order
  // issues (the classloader classes lock and the monitor lock). We
  // call this with threads suspended.
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree() || (kToSpaceOnly && !r->IsInToSpace())) {
      continue;
    }
    if (r->IsLarge()) {
      mirror::Object* obj = reinterpret_cast<mirror::Object*>(r->Begin());
      if (obj->GetClass() != nullptr) {
        callback(obj, arg);
      }
    } else if (r->IsLargeTail()) {
      // Do nothing.
    } else {
      uint8_t* pos = r->Begin();
      uint8_t* top = r->Top();
      while (pos < top) {
        mirror::Object* obj = reinterpret_cast<mirror::Object*>(pos);
        if (obj->GetClass<kDefaultVerifyFlags, kWithoutReadBarrier>() != nullptr) {
          callback(obj, arg);
          pos = reinterpret_cast<uint8_t*>(GetNextObject(obj));
        } else {
          break;
        }
      }
    }
  }
}

inline mirror::Object* RegionSpace::GetNextObject(mirror::Object* obj) {
  const uintptr_t position = reinterpret_cast<uintptr_t>(obj) + obj->SizeOf();
  return reinterpret_cast<mirror::Object*>(RoundUp(position, kAlignment));
}

template<bool kForEvac>
mirror::Object* RegionSpace::AllocLarge(size_t num_bytes, size_t* bytes_allocated,
                                        size_t* usable_size,
                                        size_t* bytes_tl_bulk_allocated) {
  DCHECK(IsAligned<kAlignment>(num_bytes));
  DCHECK_GT(num_bytes, kRegionSize);
  size_t num_regs = RoundUp(num_bytes, kRegionSize) / kRegionSize;
  DCHECK_GT(num_regs, 0U);
  DCHECK_LT((num_regs - 1) * kRegionSize, num_bytes);
  DCHECK_LE(num_bytes, num_regs * kRegionSize);
  MutexLock mu(Thread::Current(), region_lock_);
  if (!kForEvac) {
    // Retain sufficient free regions for full evacuation.
    if ((num_non_free_regions_ + num_regs) * 2 > num_regions_) {
      return nullptr;
    }
  }
  // Find a large enough contiguous free regions.
  size_t left = 0;
  while (left + num_regs - 1 < num_regions_) {
    bool found = true;
    size_t right = left;
    DCHECK_LT(right, left + num_regs)
        << "The inner loop Should iterate at least once";
    while (right < left + num_regs) {
      if (regions_[right].IsFree()) {
        ++right;
      } else {
        found = false;
        break;
      }
    }
    if (found) {
      // right points to the one region past the last free region.
      DCHECK_EQ(left + num_regs, right);
      Region* first_reg = &regions_[left];
      DCHECK(first_reg->IsFree());
      first_reg->UnfreeLarge(time_);
      ++num_non_free_regions_;
      first_reg->SetTop(first_reg->Begin() + num_bytes);
      for (size_t p = left + 1; p < right; ++p) {
        DCHECK_LT(p, num_regions_);
        DCHECK(regions_[p].IsFree());
        regions_[p].UnfreeLargeTail(time_);
        ++num_non_free_regions_;
      }
      *bytes_allocated = num_bytes;
      if (usable_size != nullptr) {
        *usable_size = num_regs * kRegionSize;
      }
      *bytes_tl_bulk_allocated = num_bytes;
      return reinterpret_cast<mirror::Object*>(first_reg->Begin());
    } else {
      // right points to the non-free region. Start with the one after it.
      left = right + 1;
    }
  }
  return nullptr;
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_
