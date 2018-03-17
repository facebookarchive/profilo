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
#include "thread-current-inl.h"

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
  DCHECK_ALIGNED(num_bytes, kAlignment);
  mirror::Object* obj;
  if (LIKELY(num_bytes <= kRegionSize)) {
    // Non-large object.
    obj = (kForEvac ? evac_region_ : current_region_)->Alloc(num_bytes,
                                                             bytes_allocated,
                                                             usable_size,
                                                             bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    MutexLock mu(Thread::Current(), region_lock_);
    // Retry with current region since another thread may have updated it.
    obj = (kForEvac ? evac_region_ : current_region_)->Alloc(num_bytes,
                                                             bytes_allocated,
                                                             usable_size,
                                                             bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    Region* r = AllocateRegion(kForEvac);
    if (LIKELY(r != nullptr)) {
      obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
      CHECK(obj != nullptr);
      // Do our allocation before setting the region, this makes sure no threads race ahead
      // and fill in the region before we allocate the object. b/63153464
      if (kForEvac) {
        evac_region_ = r;
      } else {
        current_region_ = r;
      }
      return obj;
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
  DCHECK_ALIGNED(num_bytes, kAlignment);
  uint8_t* old_top;
  uint8_t* new_top;
  do {
    old_top = top_.LoadRelaxed();
    new_top = old_top + num_bytes;
    if (UNLIKELY(new_top > end_)) {
      return nullptr;
    }
  } while (!top_.CompareExchangeWeakRelaxed(old_top, new_top));
  objects_allocated_.FetchAndAddRelaxed(1);
  DCHECK_LE(Top(), end_);
  DCHECK_LT(old_top, end_);
  DCHECK_LE(new_top, end_);
  *bytes_allocated = num_bytes;
  if (usable_size != nullptr) {
    *usable_size = num_bytes;
  }
  *bytes_tl_bulk_allocated = num_bytes;
  return reinterpret_cast<mirror::Object*>(old_top);
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

template<bool kToSpaceOnly, typename Visitor>
void RegionSpace::WalkInternal(Visitor&& visitor) {
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
      // Avoid visiting dead large objects since they may contain dangling pointers to the
      // from-space.
      DCHECK_GT(r->LiveBytes(), 0u) << "Visiting dead large object";
      mirror::Object* obj = reinterpret_cast<mirror::Object*>(r->Begin());
      DCHECK(obj->GetClass() != nullptr);
      visitor(obj);
    } else if (r->IsLargeTail()) {
      // Do nothing.
    } else {
      // For newly allocated and evacuated regions, live bytes will be -1.
      uint8_t* pos = r->Begin();
      uint8_t* top = r->Top();
      const bool need_bitmap =
          r->LiveBytes() != static_cast<size_t>(-1) &&
          r->LiveBytes() != static_cast<size_t>(top - pos);
      if (need_bitmap) {
        GetLiveBitmap()->VisitMarkedRange(
            reinterpret_cast<uintptr_t>(pos),
            reinterpret_cast<uintptr_t>(top),
            visitor);
      } else {
        while (pos < top) {
          mirror::Object* obj = reinterpret_cast<mirror::Object*>(pos);
          if (obj->GetClass<kDefaultVerifyFlags, kWithoutReadBarrier>() != nullptr) {
            visitor(obj);
            pos = reinterpret_cast<uint8_t*>(GetNextObject(obj));
          } else {
            break;
          }
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
  DCHECK_ALIGNED(num_bytes, kAlignment);
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
      first_reg->UnfreeLarge(this, time_);
      ++num_non_free_regions_;
      size_t allocated = num_regs * kRegionSize;
      // We make 'top' all usable bytes, as the caller of this
      // allocation may use all of 'usable_size' (see mirror::Array::Alloc).
      first_reg->SetTop(first_reg->Begin() + allocated);
      for (size_t p = left + 1; p < right; ++p) {
        DCHECK_LT(p, num_regions_);
        DCHECK(regions_[p].IsFree());
        regions_[p].UnfreeLargeTail(this, time_);
        ++num_non_free_regions_;
      }
      *bytes_allocated = allocated;
      if (usable_size != nullptr) {
        *usable_size = allocated;
      }
      *bytes_tl_bulk_allocated = allocated;
      return reinterpret_cast<mirror::Object*>(first_reg->Begin());
    } else {
      // right points to the non-free region. Start with the one after it.
      left = right + 1;
    }
  }
  return nullptr;
}

inline size_t RegionSpace::Region::BytesAllocated() const {
  if (IsLarge()) {
    DCHECK_LT(begin_ + kRegionSize, Top());
    return static_cast<size_t>(Top() - begin_);
  } else if (IsLargeTail()) {
    DCHECK_EQ(begin_, Top());
    return 0;
  } else {
    DCHECK(IsAllocated()) << static_cast<uint>(state_);
    DCHECK_LE(begin_, Top());
    size_t bytes;
    if (is_a_tlab_) {
      bytes = thread_->GetThreadLocalBytesAllocated();
    } else {
      bytes = static_cast<size_t>(Top() - begin_);
    }
    DCHECK_LE(bytes, kRegionSize);
    return bytes;
  }
}


}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_
