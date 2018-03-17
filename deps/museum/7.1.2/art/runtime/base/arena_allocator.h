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
#include "base/memory_tool.h"
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
  kArenaAllocSwitchTable,
  kArenaAllocSlowPaths,
  kArenaAllocGrowableBitMap,
  kArenaAllocSTL,
  kArenaAllocGraphBuilder,
  kArenaAllocGraph,
  kArenaAllocBasicBlock,
  kArenaAllocBlockList,
  kArenaAllocReversePostOrder,
  kArenaAllocLinearOrder,
  kArenaAllocConstantsMap,
  kArenaAllocPredecessors,
  kArenaAllocSuccessors,
  kArenaAllocDominated,
  kArenaAllocInstruction,
  kArenaAllocInvokeInputs,
  kArenaAllocPhiInputs,
  kArenaAllocLoopInfo,
  kArenaAllocLoopInfoBackEdges,
  kArenaAllocTryCatchInfo,
  kArenaAllocUseListNode,
  kArenaAllocEnvironment,
  kArenaAllocEnvironmentVRegs,
  kArenaAllocEnvironmentLocations,
  kArenaAllocLocationSummary,
  kArenaAllocSsaBuilder,
  kArenaAllocMoveOperands,
  kArenaAllocCodeBuffer,
  kArenaAllocStackMaps,
  kArenaAllocOptimization,
  kArenaAllocGvn,
  kArenaAllocInductionVarAnalysis,
  kArenaAllocBoundsCheckElimination,
  kArenaAllocDCE,
  kArenaAllocLSE,
  kArenaAllocLICM,
  kArenaAllocSsaLiveness,
  kArenaAllocSsaPhiElimination,
  kArenaAllocReferenceTypePropagation,
  kArenaAllocSideEffectsAnalysis,
  kArenaAllocRegisterAllocator,
  kArenaAllocRegisterAllocatorValidate,
  kArenaAllocStackMapStream,
  kArenaAllocCodeGenerator,
  kArenaAllocAssembler,
  kArenaAllocParallelMoveResolver,
  kArenaAllocGraphChecker,
  kArenaAllocVerifier,
  kArenaAllocCallingConvention,
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

  void Copy(const ArenaAllocatorStatsImpl& other ATTRIBUTE_UNUSED) {}
  void RecordAlloc(size_t bytes ATTRIBUTE_UNUSED, ArenaAllocKind kind ATTRIBUTE_UNUSED) {}
  size_t NumAllocations() const { return 0u; }
  size_t BytesAllocated() const { return 0u; }
  void Dump(std::ostream& os ATTRIBUTE_UNUSED,
            const Arena* first ATTRIBUTE_UNUSED,
            ssize_t lost_bytes_adjustment ATTRIBUTE_UNUSED) const {}
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

template <bool kAvailable, bool kValgrind>
class ArenaAllocatorMemoryToolCheckImpl {
  // This is the generic template but since there is a partial specialization
  // for kValgrind == false, this can be instantiated only for kValgrind == true.
  static_assert(kValgrind, "This template can be instantiated only for Valgrind.");
  static_assert(kAvailable, "Valgrind implies memory tool availability.");

 public:
  ArenaAllocatorMemoryToolCheckImpl() : is_running_on_valgrind_(RUNNING_ON_MEMORY_TOOL) { }
  bool IsRunningOnMemoryTool() { return is_running_on_valgrind_; }

 private:
  const bool is_running_on_valgrind_;
};

template <bool kAvailable>
class ArenaAllocatorMemoryToolCheckImpl<kAvailable, false> {
 public:
  ArenaAllocatorMemoryToolCheckImpl() { }
  bool IsRunningOnMemoryTool() { return kAvailable; }
};

typedef ArenaAllocatorMemoryToolCheckImpl<kMemoryToolIsAvailable, kMemoryToolIsValgrind>
    ArenaAllocatorMemoryToolCheck;

class ArenaAllocatorMemoryTool : private ArenaAllocatorMemoryToolCheck {
 public:
  using ArenaAllocatorMemoryToolCheck::IsRunningOnMemoryTool;

  void MakeDefined(void* ptr, size_t size) {
    if (UNLIKELY(IsRunningOnMemoryTool())) {
      DoMakeDefined(ptr, size);
    }
  }
  void MakeUndefined(void* ptr, size_t size) {
    if (UNLIKELY(IsRunningOnMemoryTool())) {
      DoMakeUndefined(ptr, size);
    }
  }
  void MakeInaccessible(void* ptr, size_t size) {
    if (UNLIKELY(IsRunningOnMemoryTool())) {
      DoMakeInaccessible(ptr, size);
    }
  }

 private:
  void DoMakeDefined(void* ptr, size_t size);
  void DoMakeUndefined(void* ptr, size_t size);
  void DoMakeInaccessible(void* ptr, size_t size);
};

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

  friend class ArenaAllocatorTest;

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
  MemMapArena(size_t size, bool low_4gb, const char* name);
  virtual ~MemMapArena();
  void Release() OVERRIDE;

 private:
  std::unique_ptr<MemMap> map_;
};

class ArenaPool {
 public:
  ArenaPool(bool use_malloc = true,
            bool low_4gb = false,
            const char* name = "LinearAlloc");
  ~ArenaPool();
  Arena* AllocArena(size_t size) REQUIRES(!lock_);
  void FreeArenaChain(Arena* first) REQUIRES(!lock_);
  size_t GetBytesAllocated() const REQUIRES(!lock_);
  void ReclaimMemory() NO_THREAD_SAFETY_ANALYSIS;
  void LockReclaimMemory() REQUIRES(!lock_);
  // Trim the maps in arenas by madvising, used by JIT to reduce memory usage. This only works
  // use_malloc is false.
  void TrimMaps() REQUIRES(!lock_);

 private:
  const bool use_malloc_;
  mutable Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  Arena* free_arenas_ GUARDED_BY(lock_);
  const bool low_4gb_;
  const char* name_;
  DISALLOW_COPY_AND_ASSIGN(ArenaPool);
};

// Fast single-threaded allocator for zero-initialized memory chunks.
//
// Memory is allocated from ArenaPool in large chunks and then rationed through
// the ArenaAllocator. It's returned to the ArenaPool only when the ArenaAllocator
// is destroyed.
class ArenaAllocator
    : private DebugStackRefCounter, private ArenaAllocatorStats, private ArenaAllocatorMemoryTool {
 public:
  explicit ArenaAllocator(ArenaPool* pool);
  ~ArenaAllocator();

  using ArenaAllocatorMemoryTool::IsRunningOnMemoryTool;
  using ArenaAllocatorMemoryTool::MakeDefined;
  using ArenaAllocatorMemoryTool::MakeUndefined;
  using ArenaAllocatorMemoryTool::MakeInaccessible;

  // Get adapter for use in STL containers. See arena_containers.h .
  ArenaAllocatorAdapter<void> Adapter(ArenaAllocKind kind = kArenaAllocSTL);

  // Returns zeroed memory.
  void* Alloc(size_t bytes, ArenaAllocKind kind = kArenaAllocMisc) ALWAYS_INLINE {
    if (UNLIKELY(IsRunningOnMemoryTool())) {
      return AllocWithMemoryTool(bytes, kind);
    }
    bytes = RoundUp(bytes, kAlignment);
    ArenaAllocatorStats::RecordAlloc(bytes, kind);
    if (UNLIKELY(bytes > static_cast<size_t>(end_ - ptr_))) {
      return AllocFromNewArena(bytes);
    }
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
      DCHECK(!IsRunningOnMemoryTool());  // Red zone prevents end == ptr_.
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
  T* Alloc(ArenaAllocKind kind = kArenaAllocMisc) {
    return AllocArray<T>(1, kind);
  }

  template <typename T>
  T* AllocArray(size_t length, ArenaAllocKind kind = kArenaAllocMisc) {
    return static_cast<T*>(Alloc(length * sizeof(T), kind));
  }

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
  void* AllocWithMemoryTool(size_t bytes, ArenaAllocKind kind);
  uint8_t* AllocFromNewArena(size_t bytes);

  static constexpr size_t kAlignment = 8;

  void UpdateBytesAllocated();

  ArenaPool* pool_;
  uint8_t* begin_;
  uint8_t* end_;
  uint8_t* ptr_;
  Arena* arena_head_;

  template <typename U>
  friend class ArenaAllocatorAdapter;

  friend class ArenaAllocatorTest;

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
