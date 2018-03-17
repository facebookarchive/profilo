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

#ifndef ART_RUNTIME_BASE_ARRAY_SLICE_H_
#define ART_RUNTIME_BASE_ARRAY_SLICE_H_

#include "stride_iterator.h"
#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/iteration_range.h"

namespace art {

// An ArraySlice is an abstraction over an array or a part of an array of a particular type. It does
// bounds checking and can be made from several common array-like structures in Art.
template <typename T>
class ArraySlice {
 public:
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = StrideIterator<T>;
  using const_iterator = StrideIterator<const T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using difference_type = ptrdiff_t;
  using size_type = size_t;

  // Create an empty array slice.
  ArraySlice() : array_(nullptr), size_(0), element_size_(0) {}

  // Create an array slice of the first 'length' elements of the array, with each element being
  // element_size bytes long.
  ArraySlice(T* array,
             size_t length,
             size_t element_size = sizeof(T))
      : array_(array),
        size_(dchecked_integral_cast<uint32_t>(length)),
        element_size_(element_size) {
    DCHECK(array_ != nullptr || length == 0);
  }

  // Iterators.
  iterator begin() { return iterator(&AtUnchecked(0), element_size_); }
  const_iterator begin() const { return const_iterator(&AtUnchecked(0), element_size_); }
  const_iterator cbegin() const { return const_iterator(&AtUnchecked(0), element_size_); }
  StrideIterator<T> end() { return StrideIterator<T>(&AtUnchecked(size_), element_size_); }
  const_iterator end() const { return const_iterator(&AtUnchecked(size_), element_size_); }
  const_iterator cend() const { return const_iterator(&AtUnchecked(size_), element_size_); }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

  // Size.
  size_type size() const { return size_; }
  bool empty() const { return size() == 0u; }

  // Element access. NOTE: Not providing at() and data().

  reference operator[](size_t index) {
    DCHECK_LT(index, size_);
    return AtUnchecked(index);
  }

  const_reference operator[](size_t index) const {
    DCHECK_LT(index, size_);
    return AtUnchecked(index);
  }

  reference front() {
    DCHECK(!empty());
    return (*this)[0];
  }

  const_reference front() const {
    DCHECK(!empty());
    return (*this)[0];
  }

  reference back() {
    DCHECK(!empty());
    return (*this)[size_ - 1u];
  }

  const_reference back() const {
    DCHECK(!empty());
    return (*this)[size_ - 1u];
  }

  ArraySlice<T> SubArray(size_type pos) {
    return SubArray(pos, size() - pos);
  }

  ArraySlice<const T> SubArray(size_type pos) const {
    return SubArray(pos, size() - pos);
  }

  ArraySlice<T> SubArray(size_type pos, size_type length) {
    DCHECK_LE(pos, size());
    DCHECK_LE(length, size() - pos);
    return ArraySlice<T>(&AtUnchecked(pos), length, element_size_);
  }

  ArraySlice<const T> SubArray(size_type pos, size_type length) const {
    DCHECK_LE(pos, size());
    DCHECK_LE(length, size() - pos);
    return ArraySlice<const T>(&AtUnchecked(pos), length, element_size_);
  }

  size_t ElementSize() const {
    return element_size_;
  }

  bool Contains(const T* element) const {
    return &AtUnchecked(0) <= element && element < &AtUnchecked(size_);
  }

 private:
  T& AtUnchecked(size_t index) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(array_) + index * element_size_);
  }

  const T& AtUnchecked(size_t index) const {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(array_) + index * element_size_);
  }

  T* array_;
  size_t size_;
  size_t element_size_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_ARRAY_SLICE_H_
