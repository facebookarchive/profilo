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

#ifndef ART_RUNTIME_BASE_ARENA_ALLOCATOR_H_
#define ART_RUNTIME_BASE_ARENA_ALLOCATOR_H_

#include <stdint.h>
#include <stddef.h>

#include "base/bit_utils.h"
#include "debug_stack.h"
#include "macros.h"
#include "mutex.h"

namespace art {

class Arena;
class ArenaPool;
class ArenaAllocator;
class ArenaStack;
class ScopedArenaAllocator;
class MemMap;
class MemStats;

template <typename T>
class ArenaAllocatorAdapter;

static constexpr bool kArenaAllocatorCountAllocations = false;

// Type of allocation for memory tuning.
enum ArenaAllocKind {
  kArenaAllocMisc,
  kArenaAllocBB,
  kArenaAllocBBList,
  kArenaAllocBBPredecessors,
  kArenaAllocDfsPreOrder,
  kArenaAllocDfsPostOrder,
  kArenaAllocDomPostOrder,
  kArenaAllocTopologicalSortOrder,
  kArenaAllocLoweringInfo,
  kArenaAllocLIR,
  kArenaAllocLIRResourceMask,
  kArenaAllocSwitchTable,
  kArenaAllocFillArrayData,
  kArenaAllocSlowPaths,
  kArenaAllocMIR,
  kArenaAllocDFInfo,
  kArenaAllocGrowableArray,
  kArenaAllocGrowableBitMap,
  kArenaAllocSSAToDalvikMap,
  kArenaAllocDalvikToSSAMap,
  kArenaAllocDebugInfo,
  kArenaAllocSuccessor,
  kArenaAllocRegAlloc,
  kArenaAllocData,
  kArenaAllocPredecessors,
  kArenaAllocSTL,
  kNumArenaAllocKinds
};

template <bool kCount>
class ArenaAllocatorStatsImpl;

template <>
class ArenaAllocatorStatsImpl<false> {
 public:
  ArenaAllocatorStatsImpl() = default;
  ArenaAllocatorStatsImpl(const ArenaAllocatorStatsImpl& other) = default;
  ArenaAllocatorStatsImpl& operator = (const ArenaAllocatorStatsImpl& other) = delete;

  void Copy(const ArenaAllocatorStatsImpl& other) { UNUSED(other); }
  void RecordAlloc(size_t bytes, ArenaAllocKind kind) { UNUSED(bytes, kind); }
  size_t NumAllocations() const { return 0u; }
  size_t BytesAllocated() const { return 0u; }
  void Dump(std::ostream& os, const Arena* first, ssize_t lost_bytes_adjustment) const {
    UNUSED(os); UNUSED(first); UNUSED(lost_bytes_adjustment);
  }
};

template <bool kCount>
class ArenaAllocatorStatsImpl {
 public:
  ArenaAllocatorStatsImpl();
  ArenaAllocatorStatsImpl(const ArenaAllocatorStatsImpl& other) = default;
  ArenaAllocatorStatsImpl& operator = (const ArenaAllocatorStatsImpl& other) = delete;

  void Copy(const ArenaAllocatorStatsImpl& other);
  void RecordAlloc(size_t bytes, ArenaAllocKind kind);
  size_t NumAllocations() const;
  size_t BytesAllocated() const;
  void Dump(std::ostream& os, const Arena* first, ssize_t lost_bytes_adjustment) const;

 private:
  size_t num_allocations_;
  // TODO: Use std::array<size_t, kNumArenaAllocKinds> from C++11 when we upgrade the STL.
  size_t alloc_stats_[kNumArenaAllocKinds];  // Bytes used by various allocation kinds.

  static const char* const kAllocNames[];
};

typedef ArenaAllocatorStatsImpl<kArenaAllocatorCountAllocations> ArenaAllocatorStats;

class Arena {
 public:
  static constexpr size_t kDefaultSize = 128 * KB;
  Arena();
  virtual ~Arena() { }
  // Reset is for pre-use and uses memset for performance.
  void Reset();
  // Release is used inbetween uses and uses madvise for memory usage.
  virtual void Release() { }
  uint8_t* Begin() {
    return memory_;
  }

  uint8_t* End() {
    return memory_ + size_;
  }

  size_t Size() const {
    return size_;
  }

  size_t RemainingSpace() const {
    return Size() - bytes_allocated_;
  }

  size_t GetBytesAllocated() const {
    return bytes_allocated_;
  }

  // Return true if ptr is contained in the arena.
  bool Contains(const void* ptr) const {
    return memory_ <= ptr && ptr < memory_ + bytes_allocated_;
  }

 protected:
  size_t bytes_allocated_;
  uint8_t* memory_;
  size_t size_;
  Arena* next_;
  friend class ArenaPool;
  friend class ArenaAllocator;
  friend class ArenaStack;
  friend class ScopedArenaAllocator;
  template <bool kCount> friend class ArenaAllocatorStatsImpl;

 private:
  DISALLOW_COPY_AND_ASSIGN(Arena);
};

class MallocArena FINAL : public Arena {
 public:
  explicit MallocArena(size_t size = Arena::kDefaultSize);
  virtual ~MallocArena();
};

class MemMapArena FINAL : public Arena {
 public:
  explicit MemMapArena(size_t size, bool low_4gb);
  virtual ~MemMapArena();
  void Release() OVERRIDE;

 private:
  std::unique_ptr<MemMap> map_;
};

class ArenaPool {
 public:
  explicit ArenaPool(bool use_malloc = true, bool low_4gb = false);
  ~ArenaPool();
  Arena* AllocArena(size_t size) LOCKS_EXCLUDED(lock_);
  void FreeArenaChain(Arena* first) LOCKS_EXCLUDED(lock_);
  size_t GetBytesAllocated() const LOCKS_EXCLUDED(lock_);
  // Trim the maps in arenas by madvising, used by JIT to reduce memory usage. This only works
  // use_malloc is false.
  void TrimMaps() LOCKS_EXCLUDED(lock_);

 private:
  const bool use_malloc_;
  mutable Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  Arena* free_arenas_ GUARDED_BY(lock_);
  const bool low_4gb_;
  DISALLOW_COPY_AND_ASSIGN(ArenaPool);
};

class ArenaAllocator : private DebugStackRefCounter, private ArenaAllocatorStats {
 public:
  explicit ArenaAllocator(ArenaPool* pool);
  ~ArenaAllocator();

  // Get adapter for use in STL containers. See arena_containers.h .
  ArenaAllocatorAdapter<void> Adapter(ArenaAllocKind kind = kArenaAllocSTL);

  // Returns zeroed memory.
  void* Alloc(size_t bytes, ArenaAllocKind kind = kArenaAllocMisc) ALWAYS_INLINE {
    if (UNLIKELY(running_on_valgrind_)) {
      return AllocValgrind(bytes, kind);
    }
    bytes = RoundUp(bytes, kAlignment);
    if (UNLIKELY(ptr_ + bytes > end_)) {
      // Obtain a new block.
      ObtainNewArenaForAllocation(bytes);
      if (UNLIKELY(ptr_ == nullptr)) {
        return nullptr;
      }
    }
    ArenaAllocatorStats::RecordAlloc(bytes, kind);
    uint8_t* ret = ptr_;
    ptr_ += bytes;
    return ret;
  }

  // Realloc never frees the input pointer, it is the caller's job to do this if necessary.
  void* Realloc(void* ptr, size_t ptr_size, size_t new_size,
                ArenaAllocKind kind = kArenaAllocMisc) ALWAYS_INLINE {
    DCHECK_GE(new_size, ptr_size);
    DCHECK_EQ(ptr == nullptr, ptr_size == 0u);
    auto* end = reinterpret_cast<uint8_t*>(ptr) + ptr_size;
    // If we haven't allocated anything else, we can safely extend.
    if (end == ptr_) {
      const size_t size_delta = new_size - ptr_size;
      // Check remain space.
      const size_t remain = end_ - ptr_;
      if (remain >= size_delta) {
        ptr_ += size_delta;
        ArenaAllocatorStats::RecordAlloc(size_delta, kind);
        return ptr;
      }
    }
    auto* new_ptr = Alloc(new_size, kind);
    memcpy(new_ptr, ptr, ptr_size);
    // TODO: Call free on ptr if linear alloc supports free.
    return new_ptr;
  }

  template <typename T>
  T* AllocArray(size_t length, ArenaAllocKind kind = kArenaAllocMisc) {
    return static_cast<T*>(Alloc(length * sizeof(T), kind));
  }

  void* AllocValgrind(size_t bytes, ArenaAllocKind kind);

  void ObtainNewArenaForAllocation(size_t allocation_size);

  size_t BytesAllocated() const;

  MemStats GetMemStats() const;

  // The BytesUsed method sums up bytes allocated from arenas in arena_head_ and nodes.
  // TODO: Change BytesAllocated to this behavior?
  size_t BytesUsed() const;

  ArenaPool* GetArenaPool() const {
    return pool_;
  }

  bool Contains(const void* ptr) const;

 private:
  static constexpr size_t kAlignment = 8;

  void UpdateBytesAllocated();

  ArenaPool* pool_;
  uint8_t* begin_;
  uint8_t* end_;
  uint8_t* ptr_;
  Arena* arena_head_;
  bool running_on_valgrind_;

  template <typename U>
  friend class ArenaAllocatorAdapter;

  DISALLOW_COPY_AND_ASSIGN(ArenaAllocator);
};  // ArenaAllocator

class MemStats {
 public:
  MemStats(const char* name, const ArenaAllocatorStats* stats, const Arena* first_arena,
           ssize_t lost_bytes_adjustment = 0);
  void Dump(std::ostream& os) const;

 private:
  const char* const name_;
  const ArenaAllocatorStats* const stats_;
  const Arena* const first_arena_;
  const ssize_t lost_bytes_adjustment_;
};  // MemStats

}  // namespace art

#endif  // ART_RUNTIME_BASE_ARENA_ALLOCATOR_H_
