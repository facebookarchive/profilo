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

#include "base/macros.h"
#include "base/mutex.h"
#include "space.h"
#include "thread.h"

namespace art {
namespace gc {

namespace accounting {
class ReadBarrierTable;
}  // namespace accounting

namespace space {

// A space that consists of equal-sized regions.
class RegionSpace FINAL : public ContinuousMemMapAllocSpace {
 public:
  typedef void(*WalkCallback)(void *start, void *end, size_t num_bytes, void* callback_arg);

  SpaceType GetType() const OVERRIDE {
    return kSpaceTypeRegionSpace;
  }

  // Create a region space mem map with the requested sizes. The requested base address is not
  // guaranteed to be granted, if it is required, the caller should call Begin on the returned
  // space to confirm the request was granted.
  static MemMap* CreateMemMap(const std::string& name, size_t capacity, uint8_t* requested_begin);
  static RegionSpace* Create(const std::string& name, MemMap* mem_map);

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
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!region_lock_) {
    return AllocationSizeNonvirtual(obj, usable_size);
  }
  size_t AllocationSizeNonvirtual(mirror::Object* obj, size_t* usable_size)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!region_lock_);

  size_t Free(Thread*, mirror::Object*) OVERRIDE {
    UNIMPLEMENTED(FATAL);
    return 0;
  }
  size_t FreeList(Thread*, size_t, mirror::Object**) OVERRIDE {
    UNIMPLEMENTED(FATAL);
    return 0;
  }
  accounting::ContinuousSpaceBitmap* GetLiveBitmap() const OVERRIDE {
    return mark_bitmap_.get();
  }
  accounting::ContinuousSpaceBitmap* GetMarkBitmap() const OVERRIDE {
    return mark_bitmap_.get();
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
  template <typename Visitor>
  ALWAYS_INLINE void Walk(Visitor&& visitor) REQUIRES(Locks::mutator_lock_) {
    WalkInternal<false /* kToSpaceOnly */>(visitor);
  }
  template <typename Visitor>
  ALWAYS_INLINE void WalkToSpace(Visitor&& visitor)
      REQUIRES(Locks::mutator_lock_) {
    WalkInternal<true>(visitor);
  }

  accounting::ContinuousSpaceBitmap::SweepCallback* GetSweepCallback() OVERRIDE {
    return nullptr;
  }
  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!region_lock_);

  // Object alignment within the space.
  static constexpr size_t kAlignment = kObjectAlignment;
  // The region size.
  static constexpr size_t kRegionSize = 256 * KB;

  bool IsInFromSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInFromSpace();
    }
    return false;
  }

  bool IsInNewlyAllocatedRegion(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsNewlyAllocated();
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
  void ClearFromSpace(uint64_t* cleared_bytes, uint64_t* cleared_objects) REQUIRES(!region_lock_);

  void AddLiveBytes(mirror::Object* ref, size_t alloc_size) {
    Region* reg = RefToRegionUnlocked(ref);
    reg->AddLiveBytes(alloc_size);
  }

  void AssertAllRegionLiveBytesZeroOrCleared() REQUIRES(!region_lock_) {
    if (kIsDebugBuild) {
      MutexLock mu(Thread::Current(), region_lock_);
      for (size_t i = 0; i < num_regions_; ++i) {
        Region* r = &regions_[i];
        size_t live_bytes = r->LiveBytes();
        CHECK(live_bytes == 0U || live_bytes == static_cast<size_t>(-1)) << live_bytes;
      }
    }
  }

  void RecordAlloc(mirror::Object* ref) REQUIRES(!region_lock_);
  bool AllocNewTlab(Thread* self, size_t min_bytes) REQUIRES(!region_lock_);

  uint32_t Time() {
    return time_;
  }

 private:
  RegionSpace(const std::string& name, MemMap* mem_map);

  template<bool kToSpaceOnly, typename Visitor>
  ALWAYS_INLINE void WalkInternal(Visitor&& visitor) NO_THREAD_SAFETY_ANALYSIS;

  class Region {
   public:
    Region()
        : idx_(static_cast<size_t>(-1)),
          begin_(nullptr), top_(nullptr), end_(nullptr),
          state_(RegionState::kRegionStateAllocated), type_(RegionType::kRegionTypeToSpace),
          objects_allocated_(0), alloc_time_(0), live_bytes_(static_cast<size_t>(-1)),
          is_newly_allocated_(false), is_a_tlab_(false), thread_(nullptr) {}

    void Init(size_t idx, uint8_t* begin, uint8_t* end) {
      idx_ = idx;
      begin_ = begin;
      top_.StoreRelaxed(begin);
      end_ = end;
      state_ = RegionState::kRegionStateFree;
      type_ = RegionType::kRegionTypeNone;
      objects_allocated_.StoreRelaxed(0);
      alloc_time_ = 0;
      live_bytes_ = static_cast<size_t>(-1);
      is_newly_allocated_ = false;
      is_a_tlab_ = false;
      thread_ = nullptr;
      DCHECK_LT(begin, end);
      DCHECK_EQ(static_cast<size_t>(end - begin), kRegionSize);
    }

    RegionState State() const {
      return state_;
    }

    RegionType Type() const {
      return type_;
    }

    void Clear(bool zero_and_release_pages);

    ALWAYS_INLINE mirror::Object* Alloc(size_t num_bytes, size_t* bytes_allocated,
                                        size_t* usable_size,
                                        size_t* bytes_tl_bulk_allocated);

    bool IsFree() const {
      bool is_free = state_ == RegionState::kRegionStateFree;
      if (is_free) {
        DCHECK(IsInNoSpace());
        DCHECK_EQ(begin_, Top());
        DCHECK_EQ(objects_allocated_.LoadRelaxed(), 0U);
      }
      return is_free;
    }

    // Given a free region, declare it non-free (allocated).
    void Unfree(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    void UnfreeLarge(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    void UnfreeLargeTail(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    void MarkAsAllocated(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

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
        DCHECK_LT(begin_ + kRegionSize, Top());
      }
      return is_large;
    }

    // Large-tail allocated.
    bool IsLargeTail() const {
      bool is_large_tail = state_ == RegionState::kRegionStateLargeTail;
      if (is_large_tail) {
        DCHECK_EQ(begin_, Top());
      }
      return is_large_tail;
    }

    size_t Idx() const {
      return idx_;
    }

    bool IsNewlyAllocated() const {
      return is_newly_allocated_;
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
      // For large allocations, we always consider all bytes in the
      // regions live.
      live_bytes_ += IsLarge() ? Top() - begin_ : live_bytes;
      DCHECK_LE(live_bytes_, BytesAllocated());
    }

    bool AllAllocatedBytesAreLive() const {
      return LiveBytes() == static_cast<size_t>(Top() - Begin());
    }

    size_t LiveBytes() const {
      return live_bytes_;
    }

    size_t BytesAllocated() const;

    size_t ObjectsAllocated() const {
      if (IsLarge()) {
        DCHECK_LT(begin_ + kRegionSize, Top());
        DCHECK_EQ(objects_allocated_.LoadRelaxed(), 0U);
        return 1;
      } else if (IsLargeTail()) {
        DCHECK_EQ(begin_, Top());
        DCHECK_EQ(objects_allocated_.LoadRelaxed(), 0U);
        return 0;
      } else {
        DCHECK(IsAllocated()) << static_cast<uint>(state_);
        return objects_allocated_;
      }
    }

    uint8_t* Begin() const {
      return begin_;
    }

    ALWAYS_INLINE uint8_t* Top() const {
      return top_.LoadRelaxed();
    }

    void SetTop(uint8_t* new_top) {
      top_.StoreRelaxed(new_top);
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
      DCHECK_EQ(objects_allocated_.LoadRelaxed(), 0U);
      DCHECK_EQ(Top(), end_);
      objects_allocated_.StoreRelaxed(num_objects);
      top_.StoreRelaxed(begin_ + num_bytes);
      DCHECK_LE(Top(), end_);
    }

   private:
    size_t idx_;                        // The region's index in the region space.
    uint8_t* begin_;                    // The begin address of the region.
    Atomic<uint8_t*> top_;              // The current position of the allocation.
    uint8_t* end_;                      // The end address of the region.
    RegionState state_;                 // The region state (see RegionState).
    RegionType type_;                   // The region type (see RegionType).
    Atomic<size_t> objects_allocated_;  // The number of objects allocated.
    uint32_t alloc_time_;               // The allocation time of the region.
    size_t live_bytes_;                 // The live bytes. Used to compute the live percent.
    bool is_newly_allocated_;           // True if it's allocated after the last collection.
    bool is_a_tlab_;                    // True if it's a tlab.
    Thread* thread_;                    // The owning thread if it's a tlab.

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
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AdjustNonFreeRegionLimit(size_t new_non_free_region_index) REQUIRES(region_lock_) {
    DCHECK_LT(new_non_free_region_index, num_regions_);
    non_free_region_index_limit_ = std::max(non_free_region_index_limit_,
                                            new_non_free_region_index + 1);
    VerifyNonFreeRegionLimit();
  }

  void SetNonFreeRegionLimit(size_t new_non_free_region_index_limit) REQUIRES(region_lock_) {
    DCHECK_LE(new_non_free_region_index_limit, num_regions_);
    non_free_region_index_limit_ = new_non_free_region_index_limit;
    VerifyNonFreeRegionLimit();
  }

  void VerifyNonFreeRegionLimit() REQUIRES(region_lock_) {
    if (kIsDebugBuild && non_free_region_index_limit_ < num_regions_) {
      for (size_t i = non_free_region_index_limit_; i < num_regions_; ++i) {
        CHECK(regions_[i].IsFree());
      }
    }
  }

  Region* AllocateRegion(bool for_evac) REQUIRES(region_lock_);

  Mutex region_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  uint32_t time_;                  // The time as the number of collections since the startup.
  size_t num_regions_;             // The number of regions in this space.
  size_t num_non_free_regions_;    // The number of non-free regions in this space.
  std::unique_ptr<Region[]> regions_ GUARDED_BY(region_lock_);
                                   // The pointer to the region array.
  // The upper-bound index of the non-free regions. Used to avoid scanning all regions in
  // SetFromSpace().  Invariant: for all i >= non_free_region_index_limit_, regions_[i].IsFree() is
  // true.
  size_t non_free_region_index_limit_ GUARDED_BY(region_lock_);
  Region* current_region_;         // The region that's being allocated currently.
  Region* evac_region_;            // The region that's being evacuated to currently.
  Region full_region_;             // The dummy/sentinel region that looks full.

  // Mark bitmap used by the GC.
  std::unique_ptr<accounting::ContinuousSpaceBitmap> mark_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(RegionSpace);
};

std::ostream& operator<<(std::ostream& os, const RegionSpace::RegionState& value);
std::ostream& operator<<(std::ostream& os, const RegionSpace::RegionType& value);

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_REGION_SPACE_H_
