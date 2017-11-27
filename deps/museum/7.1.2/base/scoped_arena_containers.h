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

#ifndef ART_RUNTIME_BASE_SCOPED_ARENA_CONTAINERS_H_
#define ART_RUNTIME_BASE_SCOPED_ARENA_CONTAINERS_H_

#include <deque>
#include <queue>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "arena_containers.h"  // For ArenaAllocatorAdapterKind.
#include "base/dchecked_vector.h"
#include "scoped_arena_allocator.h"
#include "safe_map.h"

namespace art {

// Adapter for use of ScopedArenaAllocator in STL containers.
// Use ScopedArenaAllocator::Adapter() to create an adapter to pass to container constructors.
// For example,
//   void foo(ScopedArenaAllocator* allocator) {
//     ScopedArenaVector<int> foo_vector(allocator->Adapter(kArenaAllocMisc));
//     ScopedArenaSafeMap<int, int> foo_map(std::less<int>(), allocator->Adapter());
//     // Use foo_vector and foo_map...
//   }
template <typename T>
class ScopedArenaAllocatorAdapter;

template <typename T>
using ScopedArenaDeque = std::deque<T, ScopedArenaAllocatorAdapter<T>>;

template <typename T>
using ScopedArenaQueue = std::queue<T, ScopedArenaDeque<T>>;

template <typename T>
using ScopedArenaVector = dchecked_vector<T, ScopedArenaAllocatorAdapter<T>>;

template <typename T, typename Comparator = std::less<T>>
using ScopedArenaSet = std::set<T, Comparator, ScopedArenaAllocatorAdapter<T>>;

template <typename K, typename V, typename Comparator = std::less<K>>
using ScopedArenaSafeMap =
    SafeMap<K, V, Comparator, ScopedArenaAllocatorAdapter<std::pair<const K, V>>>;

template <typename K, typename V, class Hash = std::hash<K>, class KeyEqual = std::equal_to<K>>
using ScopedArenaUnorderedMap =
    std::unordered_map<K, V, Hash, KeyEqual, ScopedArenaAllocatorAdapter<std::pair<const K, V>>>;


// Implementation details below.

template <>
class ScopedArenaAllocatorAdapter<void>
    : private DebugStackReference, private DebugStackIndirectTopRef,
      private ArenaAllocatorAdapterKind {
 public:
  typedef void value_type;
  typedef void* pointer;
  typedef const void* const_pointer;

  template <typename U>
  struct rebind {
    typedef ScopedArenaAllocatorAdapter<U> other;
  };

  explicit ScopedArenaAllocatorAdapter(ScopedArenaAllocator* arena_allocator,
                                       ArenaAllocKind kind = kArenaAllocSTL)
      : DebugStackReference(arena_allocator),
        DebugStackIndirectTopRef(arena_allocator),
        ArenaAllocatorAdapterKind(kind),
        arena_stack_(arena_allocator->arena_stack_) {
  }
  template <typename U>
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter<U>& other)
      : DebugStackReference(other),
        DebugStackIndirectTopRef(other),
        ArenaAllocatorAdapterKind(other),
        arena_stack_(other.arena_stack_) {
  }
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter&) = default;
  ScopedArenaAllocatorAdapter& operator=(const ScopedArenaAllocatorAdapter&) = default;
  ~ScopedArenaAllocatorAdapter() = default;

 private:
  ArenaStack* arena_stack_;

  template <typename U>
  friend class ScopedArenaAllocatorAdapter;
};

template <typename T>
class ScopedArenaAllocatorAdapter
    : private DebugStackReference, private DebugStackIndirectTopRef,
      private ArenaAllocatorAdapterKind {
 public:
  typedef T value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef const T* const_pointer;
  typedef const T& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <typename U>
  struct rebind {
    typedef ScopedArenaAllocatorAdapter<U> other;
  };

  explicit ScopedArenaAllocatorAdapter(ScopedArenaAllocator* arena_allocator,
                                       ArenaAllocKind kind = kArenaAllocSTL)
      : DebugStackReference(arena_allocator),
        DebugStackIndirectTopRef(arena_allocator),
        ArenaAllocatorAdapterKind(kind),
        arena_stack_(arena_allocator->arena_stack_) {
  }
  template <typename U>
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter<U>& other)
      : DebugStackReference(other),
        DebugStackIndirectTopRef(other),
        ArenaAllocatorAdapterKind(other),
        arena_stack_(other.arena_stack_) {
  }
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter&) = default;
  ScopedArenaAllocatorAdapter& operator=(const ScopedArenaAllocatorAdapter&) = default;
  ~ScopedArenaAllocatorAdapter() = default;

  size_type max_size() const {
    return static_cast<size_type>(-1) / sizeof(T);
  }

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  pointer allocate(size_type n,
                   ScopedArenaAllocatorAdapter<void>::pointer hint ATTRIBUTE_UNUSED = nullptr) {
    DCHECK_LE(n, max_size());
    DebugStackIndirectTopRef::CheckTop();
    return reinterpret_cast<T*>(arena_stack_->Alloc(n * sizeof(T),
                                                    ArenaAllocatorAdapterKind::Kind()));
  }
  void deallocate(pointer p, size_type n) {
    DebugStackIndirectTopRef::CheckTop();
    arena_stack_->MakeInaccessible(p, sizeof(T) * n);
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    // Don't CheckTop(), allow reusing existing capacity of a vector/deque below the top.
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }
  template <typename U>
  void destroy(U* p) {
    // Don't CheckTop(), allow reusing existing capacity of a vector/deque below the top.
    p->~U();
  }

 private:
  ArenaStack* arena_stack_;

  template <typename U>
  friend class ScopedArenaAllocatorAdapter;

  template <typename U>
  friend bool operator==(const ScopedArenaAllocatorAdapter<U>& lhs,
                         const ScopedArenaAllocatorAdapter<U>& rhs);
};

template <typename T>
inline bool operator==(const ScopedArenaAllocatorAdapter<T>& lhs,
                       const ScopedArenaAllocatorAdapter<T>& rhs) {
  return lhs.arena_stack_ == rhs.arena_stack_;
}

template <typename T>
inline bool operator!=(const ScopedArenaAllocatorAdapter<T>& lhs,
                       const ScopedArenaAllocatorAdapter<T>& rhs) {
  return !(lhs == rhs);
}

inline ScopedArenaAllocatorAdapter<void> ScopedArenaAllocator::Adapter(ArenaAllocKind kind) {
  return ScopedArenaAllocatorAdapter<void>(this, kind);
}

// Special deleter that only calls the destructor. Also checks for double free errors.
template <typename T>
class ArenaDelete {
  static constexpr uint8_t kMagicFill = 0xCE;

 protected:
  // Used for variable sized objects such as RegisterLine.
  ALWAYS_INLINE void ProtectMemory(T* ptr, size_t size) const {
    if (RUNNING_ON_MEMORY_TOOL > 0) {
      // Writing to the memory will fail ift we already destroyed the pointer with
      // DestroyOnlyDelete since we make it no access.
      memset(ptr, kMagicFill, size);
      MEMORY_TOOL_MAKE_NOACCESS(ptr, size);
    } else if (kIsDebugBuild) {
      CHECK(ArenaStack::ArenaTagForAllocation(reinterpret_cast<void*>(ptr)) == ArenaFreeTag::kUsed)
          << "Freeing invalid object " << ptr;
      ArenaStack::ArenaTagForAllocation(reinterpret_cast<void*>(ptr)) = ArenaFreeTag::kFree;
      // Write a magic value to try and catch use after free error.
      memset(ptr, kMagicFill, size);
    }
  }

 public:
  void operator()(T* ptr) const {
    if (ptr != nullptr) {
      ptr->~T();
      ProtectMemory(ptr, sizeof(T));
    }
  }
};

// In general we lack support for arrays. We would need to call the destructor on each element,
// which requires access to the array size. Support for that is future work.
//
// However, we can support trivially destructible component types, as then a destructor doesn't
// need to be called.
template <typename T>
class ArenaDelete<T[]> {
 public:
  void operator()(T* ptr ATTRIBUTE_UNUSED) const {
    static_assert(std::is_trivially_destructible<T>::value,
                  "ArenaUniquePtr does not support non-trivially-destructible arrays.");
    // TODO: Implement debug checks, and MEMORY_TOOL support.
  }
};

// Arena unique ptr that only calls the destructor of the element.
template <typename T>
using ArenaUniquePtr = std::unique_ptr<T, ArenaDelete<T>>;

}  // namespace art

#endif  // ART_RUNTIME_BASE_SCOPED_ARENA_CONTAINERS_H_
