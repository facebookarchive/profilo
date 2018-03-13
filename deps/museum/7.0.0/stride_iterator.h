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

#ifndef ART_RUNTIME_STRIDE_ITERATOR_H_
#define ART_RUNTIME_STRIDE_ITERATOR_H_

#include <iterator>

#include "base/logging.h"

namespace art {

template<typename T>
class StrideIterator : public std::iterator<std::forward_iterator_tag, T> {
 public:
  StrideIterator(const StrideIterator&) = default;
  StrideIterator(StrideIterator&&) = default;
  StrideIterator& operator=(const StrideIterator&) = default;
  StrideIterator& operator=(StrideIterator&&) = default;

  StrideIterator(T* ptr, size_t stride)
      : ptr_(reinterpret_cast<uintptr_t>(ptr)),
        stride_(stride) {}

  bool operator==(const StrideIterator& other) const {
    DCHECK_EQ(stride_, other.stride_);
    return ptr_ == other.ptr_;
  }

  bool operator!=(const StrideIterator& other) const {
    return !(*this == other);
  }

  StrideIterator operator++() {  // Value after modification.
    ptr_ += stride_;
    return *this;
  }

  StrideIterator operator++(int) {
    StrideIterator<T> temp = *this;
    ptr_ += stride_;
    return temp;
  }

  StrideIterator operator+(ssize_t delta) const {
    StrideIterator<T> temp = *this;
    temp += delta;
    return temp;
  }

  StrideIterator& operator+=(ssize_t delta) {
    ptr_ += static_cast<ssize_t>(stride_) * delta;
    return *this;
  }

  T& operator*() const {
    return *reinterpret_cast<T*>(ptr_);
  }

  T* operator->() const {
    return &**this;
  }

 private:
  uintptr_t ptr_;
  // Not const for operator=.
  size_t stride_;
};

}  // namespace art

#endif  // ART_RUNTIME_STRIDE_ITERATOR_H_
