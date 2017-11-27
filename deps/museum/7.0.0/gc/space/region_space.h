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

#ifndef ART_RUNTIME_GC_SPACE_REGION_SPACE_H_
#define ART_RUNTIME_GC_SPACE_REGION_SPACE_H_

#include "gc/accounting/read_barrier_table.h"
#include "object_callbacks.h"
#include "space.h"
#include "thread.h"

namespace art {
namespace gc {
namespace space {

// A space that consists of equal-sized regions.
class RegionSpace FINAL : public ContinuousMemMapAllocSpace {
 public:
  typedef void(*WalkCallback)(void *start, void *end, size_t num_bytes, void* callback_arg);

  SpaceType GetType() const OVERRIDE {
    return kSpaceTypeRegionSpace;
  }

  // Create a region space with the requested sizes. The requested base address is not
  // guaranteed to be granted, if it is required, the caller should call Begin on the returned
  // space to confirm the request was granted.
  static RegionSpace* Create(const std::string& name, size_t capacity, uint8_t* requested_begin);

  // Allocate num_bytes, returns null if the space is full.
  mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                        size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      OVERRIDE REQUIRES(!region_lock_);
  // Thread-unsafe allocation for when mutators are suspended, used by the semispace collector.
  mirror::Object* AllocThreadUnsafe(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                                    size_t* usable_size, size_t* bytes_tl_bulk_allocated)
      OVERRIDE REQUIRES(Locks::mutator_lock_) REQUIRES(!region_lock_);
  // The main allocation routine.
  template<bool kForEvac>
  ALWAYS_INLINE mirror::Object* AllocNonvirtual(size_t num_bytes, size_t* bytes_allocated,
                                                size_t* usable_size,
                                                size_t* bytes_tl_bulk_allocated)
      REQUIRES(!region_lock_);
  // Allocate/free large objects (objects that are larger than the region size.)
  template<bool kForEvac>
  mirror::Object* AllocLarge(size_t num_bytes, size_t* bytes_allocated, size_t* usable_size,
                             size_t* bytes_tl_bulk_allocated) REQUIRES(!region_lock_);
  void FreeLarge(mirror::Object* large_obj, size_t bytes_allocated) REQUIRES(!region_lock_);

  // Return the storage space required by obj.
  size_t AllocationSize(mirror::Object* obj, size_t* usable_size) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!region_lock_) {
    return AllocationSizeNonvirtual(obj, usable_size);
  }
  size_t AllocationSizeNonvirtual(mirror::Object* obj, size_t* usable_size)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!region_lock_);

  size_t Free(Thread*, mirror::Object*) OVERRIDE {
    UNIMPLEMENTED(FATAL);
    return 0;
  }
  size_t FreeList(Thread*, size_t, mirror::Object**) OVERRIDE {
    UNIMPLEMENTED(FATAL);
    return 0;
  }
  accounting::ContinuousSpaceBitmap* GetLiveBitmap() const OVERRIDE {
    // No live bitmap.
    return nullptr;
  }
  accounting::ContinuousSpaceBitmap* GetMarkBitmap() const OVERRIDE {
    // No mark bitmap.
    return nullptr;
  }

  void Clear() OVERRIDE REQUIRES(!region_lock_);

  void Dump(std::ostream& os) const;
  void DumpRegions(std::ostream& os) REQUIRES(!region_lock_);
  void DumpNonFreeRegions(std::ostream& os) REQUIRES(!region_lock_);

  size_t RevokeThreadLocalBuffers(Thread* thread) REQUIRES(!region_lock_);
  void RevokeThreadLocalBuffersLocked(Thread* thread) REQUIRES(region_lock_);
  size_t RevokeAllThreadLocalBuffers()
      REQUIRES(!Locks::runtime_shutdown_lock_, !Locks::thread_list_lock_, !region_lock_);
  void AssertThreadLocalBuffersAreRevoked(Thread* thread) REQUIRES(!region_lock_);
  void AssertAllThreadLocalBuffersAreRevoked()
      REQUIRES(!Locks::runtime_shutdown_lock_, !Locks::thread_list_lock_, !region_lock_);

  enum class RegionType : uint8_t {
    kRegionTypeAll,              // All types.
    kRegionTypeFromSpace,        // From-space. To be evacuated.
    kRegionTypeUnevacFromSpace,  // Unevacuated from-space. Not to be evacuated.
    kRegionTypeToSpace,          // To-space.
    kRegionTypeNone,             // None.
  };

  enum class RegionState : uint8_t {
    kRegionStateFree,            // Free region.
    kRegionStateAllocated,       // Allocated region.
    kRegionStateLarge,           // Large allocated (allocation larger than the region size).
    kRegionStateLargeTail,       // Large tail (non-first regions of a large allocation).
  };

  template<RegionType kRegionType> uint64_t GetBytesAllocatedInternal() REQUIRES(!region_lock_);
  template<RegionType kRegionType> uint64_t GetObjectsAllocatedInternal() REQUIRES(!region_lock_);
  uint64_t GetBytesAllocated() REQUIRES(!region_lock_) {
    return GetBytesAllocatedInternal<RegionType::kRegionTypeAll>();
  }
  uint64_t GetObjectsAllocated() REQUIRES(!region_lock_) {
    return GetObjectsAllocatedInternal<RegionType::kRegionTypeAll>();
  }
  uint64_t GetBytesAllocatedInFromSpace() REQUIRES(!region_lock_) {
    return GetBytesAllocatedInternal<RegionType::kRegionTypeFromSpace>();
  }
  uint64_t GetObjectsAllocatedInFromSpace() REQUIRES(!region_lock_) {
    return GetObjectsAllocatedInternal<RegionType::kRegionTypeFromSpace>();
  }
  uint64_t GetBytesAllocatedInUnevacFromSpace() REQUIRES(!region_lock_) {
    return GetBytesAllocatedInternal<RegionType::kRegionTypeUnevacFromSpace>();
  }
  uint64_t GetObjectsAllocatedInUnevacFromSpace() REQUIRES(!region_lock_) {
    return GetObjectsAllocatedInternal<RegionType::kRegionTypeUnevacFromSpace>();
  }

  bool CanMoveObjects() const OVERRIDE {
    return true;
  }

  bool Contains(const mirror::Object* obj) const {
    const uint8_t* byte_obj = reinterpret_cast<const uint8_t*>(obj);
    return byte_obj >= Begin() && byte_obj < Limit();
  }

  RegionSpace* AsRegionSpace() OVERRIDE {
    return this;
  }

  // Go through all of the blocks and visit the continuous objects.
  void Walk(ObjectCallback* callback, void* arg)
      REQUIRES(Locks::mutator_lock_) {
    WalkInternal<false>(callback, arg);
  }

  void WalkToSpace(ObjectCallback* callback, void* arg)
      REQUIRES(Locks::mutator_lock_) {
    WalkInternal<true>(callback, arg);
  }

  accounting::ContinuousSpaceBitmap::SweepCallback* GetSweepCallback() OVERRIDE {
    return nullptr;
  }
  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!region_lock_);

  // Object alignment within the space.
  static constexpr size_t kAlignment = kObjectAlignment;
  // The region size.
  static constexpr size_t kRegionSize = 1 * MB;

  bool IsInFromSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInFromSpace();
    }
    return false;
  }

  bool IsInUnevacFromSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInUnevacFromSpace();
    }
    return false;
  }

  bool IsInToSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInToSpace();
    }
    return false;
  }

  RegionType GetRegionType(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->Type();
    }
    return RegionType::kRegionTypeNone;
  }

  void SetFromSpace(accounting::ReadBarrierTable* rb_table, bool force_evacuate_all)
      REQUIRES(!region_lock_);

  size_t FromSpaceSize() REQUIRES(!region_lock_);
  size_t UnevacFromSpaceSize() REQUIRES(!region_lock_);
  size_t ToSpaceSize() REQUIRES(!region_lock_);
  void ClearFromSpace() REQUIRES(!region_lock_);

  void AddLiveBytes(mirror::Object* ref, size_t alloc_size) {
    Region* reg = RefToRegionUnlocked(ref);
    reg->AddLiveBytes(alloc_size);
  }

  void AssertAllRegionLiveBytesZeroOrCleared() REQUIRES(!region_lock_);

  void RecordAlloc(mirror::Object* ref) REQUIRES(!region_lock_);
  bool AllocNewTlab(Thread* self) REQUIRES(!region_lock_);

  uint32_t Time() {
    return time_;
  }

 private:
  RegionSpace(const std::string& name, MemMap* mem_map);

  template<bool kToSpaceOnly>
  void WalkInternal(ObjectCallback* callback, void* arg) NO_THREAD_SAFETY_ANALYSIS;

  class Region {
   public:
    Region()
        : idx_(static_cast<size_t>(-1)),
          begin_(nullptr), top_(nullptr), end_(nullptr),
          state_(RegionState::kRegionStateAllocated), type_(RegionType::kRegionTypeToSpace),
          objects_allocated_(0), alloc_time_(0), live_bytes_(static_cast<size_t>(-1)),
          is_newly_allocated_(false), is_a_tlab_(false), thread_(nullptr) {}

    Region(size_t idx, uint8_t* begin, uint8_t* end)
        : idx_(idx), begin_(begin), top_(begin), end_(end),
          state_(RegionState::kRegionStateFree), type_(RegionType::kRegionTypeNone),
          objects_allocated_(0), alloc_time_(0), live_bytes_(static_cast<size_t>(-1)),
          is_newly_allocated_(false), is_a_tlab_(false), thread_(nullptr) {
      DCHECK_LT(begin, end);
      DCHECK_EQ(static_cast<size_t>(end - begin), kRegionSize);
    }

    RegionState State() const {
      return state_;
    }

    RegionType Type() const {
      return type_;
    }

    void Clear() {
      top_ = begin_;
      state_ = RegionState::kRegionStateFree;
      type_ = RegionType::kRegionTypeNone;
      objects_allocated_ = 0;
      alloc_time_ = 0;
      live_bytes_ = static_cast<size_t>(-1);
      if (!kMadviseZeroes) {
        memset(begin_, 0, end_ - begin_);
      }
      madvise(begin_, end_ - begin_, MADV_DONTNEED);
      is_newly_allocated_ = false;
      is_a_tlab_ = false;
      thread_ = nullptr;
    }

    ALWAYS_INLINE mirror::Object* Alloc(size_t num_bytes, size_t* bytes_allocated,
                                        size_t* usable_size,
                                        size_t* bytes_tl_bulk_allocated);

    bool IsFree() const {
      bool is_free = state_ == RegionState::kRegionStateFree;
      if (is_free) {
        DCHECK(IsInNoSpace());
        DCHECK_EQ(begin_, top_);
        DCHECK_EQ(objects_allocated_, 0U);
      }
      return is_free;
    }

    // Given a free region, declare it non-free (allocated).
    void Unfree(uint32_t alloc_time) {
      DCHECK(IsFree());
      state_ = RegionState::kRegionStateAllocated;
      type_ = RegionType::kRegionTypeToSpace;
      alloc_time_ = alloc_time;
    }

    void UnfreeLarge(uint32_t alloc_time) {
      DCHECK(IsFree());
      state_ = RegionState::kRegionStateLarge;
      type_ = RegionType::kRegionTypeToSpace;
      alloc_time_ = alloc_time;
    }

    void UnfreeLargeTail(uint32_t alloc_time) {
      DCHECK(IsFree());
      state_ = RegionState::kRegionStateLargeTail;
      type_ = RegionType::kRegionTypeToSpace;
      alloc_time_ = alloc_time;
    }

    void SetNewlyAllocated() {
      is_newly_allocated_ = true;
    }

    // Non-large, non-large-tail allocated.
    bool IsAllocated() const {
      return state_ == RegionState::kRegionStateAllocated;
    }

    // Large allocated.
    bool IsLarge() const {
      bool is_large = state_ == RegionState::kRegionStateLarge;
      if (is_large) {
        DCHECK_LT(begin_ + 1 * MB, top_);
      }
      return is_large;
    }

    // Large-tail allocated.
    bool IsLargeTail() const {
      bool is_large_tail = state_ == RegionState::kRegionStateLargeTail;
      if (is_large_tail) {
        DCHECK_EQ(begin_, top_);
      }
      return is_large_tail;
    }

    size_t Idx() const {
      return idx_;
    }

    bool IsInFromSpace() const {
      return type_ == RegionType::kRegionTypeFromSpace;
    }

    bool IsInToSpace() const {
      return type_ == RegionType::kRegionTypeToSpace;
    }

    bool IsInUnevacFromSpace() const {
      return type_ == RegionType::kRegionTypeUnevacFromSpace;
    }

    bool IsInNoSpace() const {
      return type_ == RegionType::kRegionTypeNone;
    }

    void SetAsFromSpace() {
      DCHECK(!IsFree() && IsInToSpace());
      type_ = RegionType::kRegionTypeFromSpace;
      live_bytes_ = static_cast<size_t>(-1);
    }

    void SetAsUnevacFromSpace() {
      DCHECK(!IsFree() && IsInToSpace());
      type_ = RegionType::kRegionTypeUnevacFromSpace;
      live_bytes_ = 0U;
    }

    void SetUnevacFromSpaceAsToSpace() {
      DCHECK(!IsFree() && IsInUnevacFromSpace());
      type_ = RegionType::kRegionTypeToSpace;
    }

    ALWAYS_INLINE bool ShouldBeEvacuated();

    void AddLiveBytes(size_t live_bytes) {
      DCHECK(IsInUnevacFromSpace());
      DCHECK(!IsLargeTail());
      DCHECK_NE(live_bytes_, static_cast<size_t>(-1));
      live_bytes_ += live_bytes;
      DCHECK_LE(live_bytes_, BytesAllocated());
    }

    size_t LiveBytes() const {
      return live_bytes_;
    }

    uint GetLivePercent() const {
      DCHECK(IsInToSpace());
      DCHECK(!IsLargeTail());
      DCHECK_NE(live_bytes_, static_cast<size_t>(-1));
      DCHECK_LE(live_bytes_, BytesAllocated());
      size_t bytes_allocated = RoundUp(BytesAllocated(), kRegionSize);
      DCHECK_GE(bytes_allocated, 0U);
      uint result = (live_bytes_ * 100U) / bytes_allocated;
      DCHECK_LE(result, 100U);
      return result;
    }

    size_t BytesAllocated() const {
      if (IsLarge()) {
        DCHECK_LT(begin_ + kRegionSize, top_);
        return static_cast<size_t>(top_ - begin_);
      } else if (IsLargeTail()) {
        DCHECK_EQ(begin_, top_);
        return 0;
      } else {
        DCHECK(IsAllocated()) << static_cast<uint>(state_);
        DCHECK_LE(begin_, top_);
        size_t bytes = static_cast<size_t>(top_ - begin_);
        DCHECK_LE(bytes, kRegionSize);
        return bytes;
      }
    }

    size_t ObjectsAllocated() const {
      if (IsLarge()) {
        DCHECK_LT(begin_ + 1 * MB, top_);
        DCHECK_EQ(objects_allocated_, 0U);
        return 1;
      } else if (IsLargeTail()) {
        DCHECK_EQ(begin_, top_);
        DCHECK_EQ(objects_allocated_, 0U);
        return 0;
      } else {
        DCHECK(IsAllocated()) << static_cast<uint>(state_);
        return objects_allocated_;
      }
    }

    uint8_t* Begin() const {
      return begin_;
    }

    uint8_t* Top() const {
      return top_;
    }

    void SetTop(uint8_t* new_top) {
      top_ = new_top;
    }

    uint8_t* End() const {
      return end_;
    }

    bool Contains(mirror::Object* ref) const {
      return begin_ <= reinterpret_cast<uint8_t*>(ref) && reinterpret_cast<uint8_t*>(ref) < end_;
    }

    void Dump(std::ostream& os) const;

    void RecordThreadLocalAllocations(size_t num_objects, size_t num_bytes) {
      DCHECK(IsAllocated());
      DCHECK_EQ(objects_allocated_, 0U);
      DCHECK_EQ(top_, end_);
      objects_allocated_ = num_objects;
      top_ = begin_ + num_bytes;
      DCHECK_EQ(top_, end_);
    }

   private:
    size_t idx_;                   // The region's index in the region space.
    uint8_t* begin_;               // The begin address of the region.
    // Can't use Atomic<uint8_t*> as Atomic's copy operator is implicitly deleted.
    uint8_t* top_;                 // The current position of the allocation.
    uint8_t* end_;                 // The end address of the region.
    RegionState state_;            // The region state (see RegionState).
    RegionType type_;              // The region type (see RegionType).
    uint64_t objects_allocated_;   // The number of objects allocated.
    uint32_t alloc_time_;          // The allocation time of the region.
    size_t live_bytes_;            // The live bytes. Used to compute the live percent.
    bool is_newly_allocated_;      // True if it's allocated after the last collection.
    bool is_a_tlab_;               // True if it's a tlab.
    Thread* thread_;               // The owning thread if it's a tlab.

    friend class RegionSpace;
  };

  Region* RefToRegion(mirror::Object* ref) REQUIRES(!region_lock_) {
    MutexLock mu(Thread::Current(), region_lock_);
    return RefToRegionLocked(ref);
  }

  Region* RefToRegionUnlocked(mirror::Object* ref) NO_THREAD_SAFETY_ANALYSIS {
    // For a performance reason (this is frequently called via
    // IsInFromSpace() etc.) we avoid taking a lock here. Note that
    // since we only change a region from to-space to from-space only
    // during a pause (SetFromSpace()) and from from-space to free
    // (after GC is done) as long as ref is a valid reference into an
    // allocated region, it's safe to access the region state without
    // the lock.
    return RefToRegionLocked(ref);
  }

  Region* RefToRegionLocked(mirror::Object* ref) REQUIRES(region_lock_) {
    DCHECK(HasAddress(ref));
    uintptr_t offset = reinterpret_cast<uintptr_t>(ref) - reinterpret_cast<uintptr_t>(Begin());
    size_t reg_idx = offset / kRegionSize;
    DCHECK_LT(reg_idx, num_regions_);
    Region* reg = &regions_[reg_idx];
    DCHECK_EQ(reg->Idx(), reg_idx);
    DCHECK(reg->Contains(ref));
    return reg;
  }

  mirror::Object* GetNextObject(mirror::Object* obj)
      SHARED_REQUIRES(Locks::mutator_lock_);

  Mutex region_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  uint32_t time_;                  // The time as the number of collections since the startup.
  size_t num_regions_;             // The number of regions in this space.
  size_t num_non_free_regions_;    // The number of non-free regions in this space.
  std::unique_ptr<Region[]> regions_ GUARDED_BY(region_lock_);
                                   // The pointer to the region array.
  Region* current_region_;         // The region that's being allocated currently.
  Region* evac_region_;            // The region that's being evacuated to currently.
  Region full_region_;             // The dummy/sentinel region that looks full.

  DISALLOW_COPY_AND_ASSIGN(RegionSpace);
};

std::ostream& operator<<(std::ostream& os, const RegionSpace::RegionState& value);
std::ostream& operator<<(std::ostream& os, const RegionSpace::RegionType& value);

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_REGION_SPACE_H_
