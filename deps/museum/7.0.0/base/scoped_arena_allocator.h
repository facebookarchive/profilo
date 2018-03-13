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

#ifndef ART_RUNTIME_BASE_SCOPED_ARENA_ALLOCATOR_H_
#define ART_RUNTIME_BASE_SCOPED_ARENA_ALLOCATOR_H_

#include "arena_allocator.h"
#include "debug_stack.h"
#include "globals.h"
#include "logging.h"
#include "macros.h"

namespace art {

class ArenaStack;
class ScopedArenaAllocator;

template <typename T>
class ScopedArenaAllocatorAdapter;

// Tag associated with each allocation to help prevent double free.
enum class ArenaFreeTag : uint8_t {
  // Allocation is used and has not yet been destroyed.
  kUsed,
  // Allocation has been destroyed.
  kFree,
};

static constexpr size_t kArenaAlignment = 8;

// Holds a list of Arenas for use by ScopedArenaAllocator stack.
// The memory is returned to the ArenaPool when the ArenaStack is destroyed.
class ArenaStack : private DebugStackRefCounter, private ArenaAllocatorMemoryTool {
 public:
  explicit ArenaStack(ArenaPool* arena_pool);
  ~ArenaStack();

  using ArenaAllocatorMemoryTool::IsRunningOnMemoryTool;
  using ArenaAllocatorMemoryTool::MakeDefined;
  using ArenaAllocatorMemoryTool::MakeUndefined;
  using ArenaAllocatorMemoryTool::MakeInaccessible;

  void Reset();

  size_t PeakBytesAllocated() {
    return PeakStats()->BytesAllocated();
  }

  MemStats GetPeakStats() const;

  // Return the arena tag associated with a pointer.
  static ArenaFreeTag& ArenaTagForAllocation(void* ptr) {
    DCHECK(kIsDebugBuild) << "Only debug builds have tags";
    return *(reinterpret_cast<ArenaFreeTag*>(ptr) - 1);
  }

 private:
  struct Peak;
  struct Current;
  template <typename Tag> struct TaggedStats : ArenaAllocatorStats { };
  struct StatsAndPool : TaggedStats<Peak>, TaggedStats<Current> {
    explicit StatsAndPool(ArenaPool* arena_pool) : pool(arena_pool) { }
    ArenaPool* const pool;
  };

  ArenaAllocatorStats* PeakStats() {
    return static_cast<TaggedStats<Peak>*>(&stats_and_pool_);
  }

  ArenaAllocatorStats* CurrentStats() {
    return static_cast<TaggedStats<Current>*>(&stats_and_pool_);
  }

  // Private - access via ScopedArenaAllocator or ScopedArenaAllocatorAdapter.
  void* Alloc(size_t bytes, ArenaAllocKind kind) ALWAYS_INLINE {
    if (UNLIKELY(IsRunningOnMemoryTool())) {
      return AllocWithMemoryTool(bytes, kind);
    }
    // Add kArenaAlignment for the free or used tag. Required to preserve alignment.
    size_t rounded_bytes = RoundUp(bytes + (kIsDebugBuild ? kArenaAlignment : 0u), kArenaAlignment);
    uint8_t* ptr = top_ptr_;
    if (UNLIKELY(static_cast<size_t>(top_end_ - ptr) < rounded_bytes)) {
      ptr = AllocateFromNextArena(rounded_bytes);
    }
    CurrentStats()->RecordAlloc(bytes, kind);
    top_ptr_ = ptr + rounded_bytes;
    if (kIsDebugBuild) {
      ptr += kArenaAlignment;
      ArenaTagForAllocation(ptr) = ArenaFreeTag::kUsed;
    }
    return ptr;
  }

  uint8_t* AllocateFromNextArena(size_t rounded_bytes);
  void UpdatePeakStatsAndRestore(const ArenaAllocatorStats& restore_stats);
  void UpdateBytesAllocated();
  void* AllocWithMemoryTool(size_t bytes, ArenaAllocKind kind);

  StatsAndPool stats_and_pool_;
  Arena* bottom_arena_;
  Arena* top_arena_;
  uint8_t* top_ptr_;
  uint8_t* top_end_;

  friend class ScopedArenaAllocator;
  template <typename T>
  friend class ScopedArenaAllocatorAdapter;

  DISALLOW_COPY_AND_ASSIGN(ArenaStack);
};

// Fast single-threaded allocator. Allocated chunks are _not_ guaranteed to be zero-initialized.
//
// Unlike the ArenaAllocator, ScopedArenaAllocator is intended for relatively short-lived
// objects and allows nesting multiple allocators. Only the top allocator can be used but
// once it's destroyed, its memory can be reused by the next ScopedArenaAllocator on the
// stack. This is facilitated by returning the memory to the ArenaStack.
class ScopedArenaAllocator
    : private DebugStackReference, private DebugStackRefCounter, private ArenaAllocatorStats {
 public:
  // Create a ScopedArenaAllocator directly on the ArenaStack when the scope of
  // the allocator is not exactly a C++ block scope. For example, an optimization
  // pass can create the scoped allocator in Start() and destroy it in End().
  static ScopedArenaAllocator* Create(ArenaStack* arena_stack) {
    void* addr = arena_stack->Alloc(sizeof(ScopedArenaAllocator), kArenaAllocMisc);
    ScopedArenaAllocator* allocator = new(addr) ScopedArenaAllocator(arena_stack);
    allocator->mark_ptr_ = reinterpret_cast<uint8_t*>(addr);
    return allocator;
  }

  explicit ScopedArenaAllocator(ArenaStack* arena_stack);
  ~ScopedArenaAllocator();

  void Reset();

  void* Alloc(size_t bytes, ArenaAllocKind kind = kArenaAllocMisc) ALWAYS_INLINE {
    DebugStackReference::CheckTop();
    return arena_stack_->Alloc(bytes, kind);
  }

  template <typename T>
  T* Alloc(ArenaAllocKind kind = kArenaAllocMisc) {
    return AllocArray<T>(1, kind);
  }

  template <typename T>
  T* AllocArray(size_t length, ArenaAllocKind kind = kArenaAllocMisc) {
    return static_cast<T*>(Alloc(length * sizeof(T), kind));
  }

  // Get adapter for use in STL containers. See scoped_arena_containers.h .
  ScopedArenaAllocatorAdapter<void> Adapter(ArenaAllocKind kind = kArenaAllocSTL);

  // Allow a delete-expression to destroy but not deallocate allocators created by Create().
  static void operator delete(void* ptr ATTRIBUTE_UNUSED) {}

 private:
  ArenaStack* const arena_stack_;
  Arena* mark_arena_;
  uint8_t* mark_ptr_;
  uint8_t* mark_end_;

  void DoReset();

  template <typename T>
  friend class ScopedArenaAllocatorAdapter;

  DISALLOW_COPY_AND_ASSIGN(ScopedArenaAllocator);
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_SCOPED_ARENA_ALLOCATOR_H_
