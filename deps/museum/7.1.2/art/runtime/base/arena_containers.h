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

#ifndef ART_RUNTIME_BASE_ARENA_CONTAINERS_H_
#define ART_RUNTIME_BASE_ARENA_CONTAINERS_H_

#include <deque>
#include <queue>
#include <set>
#include <utility>

#include "arena_allocator.h"
#include "base/dchecked_vector.h"
#include "hash_map.h"
#include "hash_set.h"
#include "safe_map.h"

namespace art {

// Adapter for use of ArenaAllocator in STL containers.
// Use ArenaAllocator::Adapter() to create an adapter to pass to container constructors.
// For example,
//   struct Foo {
//     explicit Foo(ArenaAllocator* allocator)
//         : foo_vector(allocator->Adapter(kArenaAllocMisc)),
//           foo_map(std::less<int>(), allocator->Adapter()) {
//     }
//     ArenaVector<int> foo_vector;
//     ArenaSafeMap<int, int> foo_map;
//   };
template <typename T>
class ArenaAllocatorAdapter;

template <typename T>
using ArenaDeque = std::deque<T, ArenaAllocatorAdapter<T>>;

template <typename T>
using ArenaQueue = std::queue<T, ArenaDeque<T>>;

template <typename T>
using ArenaVector = dchecked_vector<T, ArenaAllocatorAdapter<T>>;

template <typename T, typename Comparator = std::less<T>>
using ArenaSet = std::set<T, Comparator, ArenaAllocatorAdapter<T>>;

template <typename K, typename V, typename Comparator = std::less<K>>
using ArenaSafeMap =
    SafeMap<K, V, Comparator, ArenaAllocatorAdapter<std::pair<const K, V>>>;

template <typename T,
          typename EmptyFn = DefaultEmptyFn<T>,
          typename HashFn = std::hash<T>,
          typename Pred = std::equal_to<T>>
using ArenaHashSet = HashSet<T, EmptyFn, HashFn, Pred, ArenaAllocatorAdapter<T>>;

template <typename Key,
          typename Value,
          typename EmptyFn = DefaultEmptyFn<std::pair<Key, Value>>,
          typename HashFn = std::hash<Key>,
          typename Pred = std::equal_to<Key>>
using ArenaHashMap = HashMap<Key,
                             Value,
                             EmptyFn,
                             HashFn,
                             Pred,
                             ArenaAllocatorAdapter<std::pair<Key, Value>>>;

// Implementation details below.

template <bool kCount>
class ArenaAllocatorAdapterKindImpl;

template <>
class ArenaAllocatorAdapterKindImpl<false> {
 public:
  // Not tracking allocations, ignore the supplied kind and arbitrarily provide kArenaAllocSTL.
  explicit ArenaAllocatorAdapterKindImpl(ArenaAllocKind kind ATTRIBUTE_UNUSED) {}
  ArenaAllocatorAdapterKindImpl(const ArenaAllocatorAdapterKindImpl&) = default;
  ArenaAllocatorAdapterKindImpl& operator=(const ArenaAllocatorAdapterKindImpl&) = default;
  ArenaAllocKind Kind() { return kArenaAllocSTL; }
};

template <bool kCount>
class ArenaAllocatorAdapterKindImpl {
 public:
  explicit ArenaAllocatorAdapterKindImpl(ArenaAllocKind kind) : kind_(kind) { }
  ArenaAllocatorAdapterKindImpl(const ArenaAllocatorAdapterKindImpl&) = default;
  ArenaAllocatorAdapterKindImpl& operator=(const ArenaAllocatorAdapterKindImpl&) = default;
  ArenaAllocKind Kind() { return kind_; }

 private:
  ArenaAllocKind kind_;
};

typedef ArenaAllocatorAdapterKindImpl<kArenaAllocatorCountAllocations> ArenaAllocatorAdapterKind;

template <>
class ArenaAllocatorAdapter<void> : private ArenaAllocatorAdapterKind {
 public:
  typedef void value_type;
  typedef void* pointer;
  typedef const void* const_pointer;

  template <typename U>
  struct rebind {
    typedef ArenaAllocatorAdapter<U> other;
  };

  explicit ArenaAllocatorAdapter(ArenaAllocator* arena_allocator,
                                 ArenaAllocKind kind = kArenaAllocSTL)
      : ArenaAllocatorAdapterKind(kind),
        arena_allocator_(arena_allocator) {
  }
  template <typename U>
  ArenaAllocatorAdapter(const ArenaAllocatorAdapter<U>& other)
      : ArenaAllocatorAdapterKind(other),
        arena_allocator_(other.arena_allocator_) {
  }
  ArenaAllocatorAdapter(const ArenaAllocatorAdapter&) = default;
  ArenaAllocatorAdapter& operator=(const ArenaAllocatorAdapter&) = default;
  ~ArenaAllocatorAdapter() = default;

 private:
  ArenaAllocator* arena_allocator_;

  template <typename U>
  friend class ArenaAllocatorAdapter;
};

template <typename T>
class ArenaAllocatorAdapter : private ArenaAllocatorAdapterKind {
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
    typedef ArenaAllocatorAdapter<U> other;
  };

  ArenaAllocatorAdapter(ArenaAllocator* arena_allocator, ArenaAllocKind kind)
      : ArenaAllocatorAdapterKind(kind),
        arena_allocator_(arena_allocator) {
  }
  template <typename U>
  ArenaAllocatorAdapter(const ArenaAllocatorAdapter<U>& other)
      : ArenaAllocatorAdapterKind(other),
        arena_allocator_(other.arena_allocator_) {
  }
  ArenaAllocatorAdapter(const ArenaAllocatorAdapter&) = default;
  ArenaAllocatorAdapter& operator=(const ArenaAllocatorAdapter&) = default;
  ~ArenaAllocatorAdapter() = default;

  size_type max_size() const {
    return static_cast<size_type>(-1) / sizeof(T);
  }

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  pointer allocate(size_type n,
                   ArenaAllocatorAdapter<void>::pointer hint ATTRIBUTE_UNUSED = nullptr) {
    DCHECK_LE(n, max_size());
    return arena_allocator_->AllocArray<T>(n, ArenaAllocatorAdapterKind::Kind());
  }
  void deallocate(pointer p, size_type n) {
    arena_allocator_->MakeInaccessible(p, sizeof(T) * n);
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }
  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

 private:
  ArenaAllocator* arena_allocator_;

  template <typename U>
  friend class ArenaAllocatorAdapter;

  template <typename U>
  friend bool operator==(const ArenaAllocatorAdapter<U>& lhs,
                         const ArenaAllocatorAdapter<U>& rhs);
};

template <typename T>
inline bool operator==(const ArenaAllocatorAdapter<T>& lhs,
                       const ArenaAllocatorAdapter<T>& rhs) {
  return lhs.arena_allocator_ == rhs.arena_allocator_;
}

template <typename T>
inline bool operator!=(const ArenaAllocatorAdapter<T>& lhs,
                       const ArenaAllocatorAdapter<T>& rhs) {
  return !(lhs == rhs);
}

inline ArenaAllocatorAdapter<void> ArenaAllocator::Adapter(ArenaAllocKind kind) {
  return ArenaAllocatorAdapter<void>(this, kind);
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_ARENA_CONTAINERS_H_
