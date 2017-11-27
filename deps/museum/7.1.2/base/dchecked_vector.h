/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_DCHECKED_VECTOR_H_
#define ART_RUNTIME_BASE_DCHECKED_VECTOR_H_

#include <algorithm>
#include <type_traits>
#include <vector>

#include "base/logging.h"

namespace art {

// Template class serving as a replacement for std::vector<> but adding
// DCHECK()s for the subscript operator, front(), back(), pop_back(),
// and for insert()/emplace()/erase() positions.
//
// Note: The element accessor at() is specified as throwing std::out_of_range
// but we do not use exceptions, so this accessor is deliberately hidden.
// Note: The common pattern &v[0] used to retrieve pointer to the data is not
// valid for an empty dchecked_vector<>. Use data() to avoid checking empty().
template <typename T, typename Alloc = std::allocator<T>>
class dchecked_vector : private std::vector<T, Alloc> {
 private:
  // std::vector<> has a slightly different specialization for bool. We don't provide that.
  static_assert(!std::is_same<T, bool>::value, "Not implemented for bool.");
  using Base = std::vector<T, Alloc>;

 public:
  using typename Base::value_type;
  using typename Base::allocator_type;
  using typename Base::reference;
  using typename Base::const_reference;
  using typename Base::pointer;
  using typename Base::const_pointer;
  using typename Base::iterator;
  using typename Base::const_iterator;
  using typename Base::reverse_iterator;
  using typename Base::const_reverse_iterator;
  using typename Base::size_type;
  using typename Base::difference_type;

  // Construct/copy/destroy.
  dchecked_vector()
      : Base() { }
  explicit dchecked_vector(const allocator_type& alloc)
      : Base(alloc) { }
  // Note that we cannot forward to std::vector(size_type, const allocator_type&) because it is not
  // available in C++11, which is the latest GCC can support. http://b/25022512
  explicit dchecked_vector(size_type n, const allocator_type& alloc = allocator_type())
      : Base(alloc) { resize(n); }
  dchecked_vector(size_type n,
                  const value_type& value,
                  const allocator_type& alloc = allocator_type())
      : Base(n, value, alloc) { }
  template <typename InputIterator>
  dchecked_vector(InputIterator first,
                  InputIterator last,
                  const allocator_type& alloc = allocator_type())
      : Base(first, last, alloc) { }
  dchecked_vector(const dchecked_vector& src)
      : Base(src) { }
  dchecked_vector(const dchecked_vector& src, const allocator_type& alloc)
      : Base(src, alloc) { }
  dchecked_vector(dchecked_vector&& src)
      : Base(std::move(src)) { }
  dchecked_vector(dchecked_vector&& src, const allocator_type& alloc)
      : Base(std::move(src), alloc) { }
  dchecked_vector(std::initializer_list<value_type> il,
                  const allocator_type& alloc = allocator_type())
      : Base(il, alloc) { }
  ~dchecked_vector() = default;
  dchecked_vector& operator=(const dchecked_vector& src) {
    Base::operator=(src);
    return *this;
  }
  dchecked_vector& operator=(dchecked_vector&& src) {
    Base::operator=(std::move(src));
    return *this;
  }
  dchecked_vector& operator=(std::initializer_list<value_type> il) {
    Base::operator=(il);
    return *this;
  }

  // Iterators.
  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;
  using Base::cbegin;
  using Base::cend;
  using Base::crbegin;
  using Base::crend;

  // Capacity.
  using Base::size;
  using Base::max_size;
  using Base::resize;
  using Base::capacity;
  using Base::empty;
  using Base::reserve;
  using Base::shrink_to_fit;

  // Element access: inherited.
  // Note: Deliberately not providing at().
  using Base::data;

  // Element access: subscript operator. Check index.
  reference operator[](size_type n) {
    DCHECK_LT(n, size());
    return Base::operator[](n);
  }
  const_reference operator[](size_type n) const {
    DCHECK_LT(n, size());
    return Base::operator[](n);
  }

  // Element access: front(), back(). Check not empty.
  reference front() { DCHECK(!empty()); return Base::front(); }
  const_reference front() const { DCHECK(!empty()); return Base::front(); }
  reference back() { DCHECK(!empty()); return Base::back(); }
  const_reference back() const { DCHECK(!empty()); return Base::back(); }

  // Modifiers: inherited.
  using Base::assign;
  using Base::push_back;
  using Base::clear;
  using Base::emplace_back;

  // Modifiers: pop_back(). Check not empty.
  void pop_back() { DCHECK(!empty()); Base::pop_back(); }

  // Modifiers: swap(). Swap only with another dchecked_vector instead of a plain vector.
  void swap(dchecked_vector& other) { Base::swap(other); }

  // Modifiers: insert(). Check position.
  iterator insert(const_iterator position, const value_type& value) {
    DCHECK(cbegin() <= position && position <= cend());
    return Base::insert(position, value);
  }
  iterator insert(const_iterator position, size_type n, const value_type& value) {
    DCHECK(cbegin() <= position && position <= cend());
    return Base::insert(position, n, value);
  }
  template <typename InputIterator>
  iterator insert(const_iterator position, InputIterator first, InputIterator last) {
    DCHECK(cbegin() <= position && position <= cend());
    return Base::insert(position, first, last);
  }
  iterator insert(const_iterator position, value_type&& value) {
    DCHECK(cbegin() <= position && position <= cend());
    return Base::insert(position, std::move(value));
  }
  iterator insert(const_iterator position, std::initializer_list<value_type> il) {
    DCHECK(cbegin() <= position && position <= cend());
    return Base::insert(position, il);
  }

  // Modifiers: erase(). Check position.
  iterator erase(const_iterator position) {
    DCHECK(cbegin() <= position && position < cend());
    return Base::erase(position);
  }
  iterator erase(const_iterator first, const_iterator last) {
    DCHECK(cbegin() <= first && first <= cend());
    DCHECK(first <= last && last <= cend());
    return Base::erase(first, last);
  }

  // Modifiers: emplace(). Check position.
  template <typename... Args>
  iterator emplace(const_iterator position, Args&&... args) {
    DCHECK(cbegin() <= position && position <= cend());
    Base::emplace(position, std::forward(args...));
  }

  // Allocator.
  using Base::get_allocator;
};

// Non-member swap(), found by argument-dependent lookup for an unqualified call.
template <typename T, typename Alloc>
void swap(dchecked_vector<T, Alloc>& lhs, dchecked_vector<T, Alloc>& rhs) {
  lhs.swap(rhs);
}

// Non-member relational operators.
template <typename T, typename Alloc>
bool operator==(const dchecked_vector<T, Alloc>& lhs, const dchecked_vector<T, Alloc>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}
template <typename T, typename Alloc>
bool operator!=(const dchecked_vector<T, Alloc>& lhs, const dchecked_vector<T, Alloc>& rhs) {
  return !(lhs == rhs);
}
template <typename T, typename Alloc>
bool operator<(const dchecked_vector<T, Alloc>& lhs, const dchecked_vector<T, Alloc>& rhs) {
  return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
template <typename T, typename Alloc>
bool operator<=(const dchecked_vector<T, Alloc>& lhs, const dchecked_vector<T, Alloc>& rhs) {
  return !(rhs < lhs);
}
template <typename T, typename Alloc>
bool operator>(const dchecked_vector<T, Alloc>& lhs, const dchecked_vector<T, Alloc>& rhs) {
  return rhs < lhs;
}
template <typename T, typename Alloc>
bool operator>=(const dchecked_vector<T, Alloc>& lhs, const dchecked_vector<T, Alloc>& rhs) {
  return !(lhs < rhs);
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_DCHECKED_VECTOR_H_
