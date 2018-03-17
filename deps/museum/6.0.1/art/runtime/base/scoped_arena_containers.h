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
#include <unordered_map>
#include <vector>

#include "arena_containers.h"  // For ArenaAllocatorAdapterKind.
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
using ScopedArenaVector = std::vector<T, ScopedArenaAllocatorAdapter<T>>;

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

  pointer allocate(size_type n, ScopedArenaAllocatorAdapter<void>::pointer hint = nullptr) {
    UNUSED(hint);
    DCHECK_LE(n, max_size());
    DebugStackIndirectTopRef::CheckTop();
    return reinterpret_cast<T*>(arena_stack_->Alloc(n * sizeof(T),
                                                    ArenaAllocatorAdapterKind::Kind()));
  }
  void deallocate(pointer p, size_type n) {
    UNUSED(p);
    UNUSED(n);
    DebugStackIndirectTopRef::CheckTop();
  }

  void construct(pointer p, const_reference val) {
    // Don't CheckTop(), allow reusing existing capacity of a vector/deque below the top.
    new (static_cast<void*>(p)) value_type(val);
  }
  void destroy(pointer p) {
    // Don't CheckTop(), allow reusing existing capacity of a vector/deque below the top.
    p->~value_type();
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

}  // namespace art

#endif  // ART_RUNTIME_BASE_SCOPED_ARENA_CONTAINERS_H_
