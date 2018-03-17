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

#ifndef ART_RUNTIME_BASE_BIT_UTILS_ITERATOR_H_
#define ART_RUNTIME_BASE_BIT_UTILS_ITERATOR_H_

#include <iterator>
#include <limits>
#include <type_traits>

#include "base/bit_utils.h"
#include "base/iteration_range.h"
#include "base/logging.h"
#include "base/stl_util.h"

namespace art {

// Using the Curiously Recurring Template Pattern to implement everything shared
// by LowToHighBitIterator and HighToLowBitIterator, i.e. everything but operator*().
template <typename T, typename Iter>
class BitIteratorBase
    : public std::iterator<std::forward_iterator_tag, uint32_t, ptrdiff_t, void, void> {
  static_assert(std::is_integral<T>::value, "T must be integral");
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");

  static_assert(sizeof(T) == sizeof(uint32_t) || sizeof(T) == sizeof(uint64_t), "Unsupported size");

 public:
  BitIteratorBase() : bits_(0u) { }
  explicit BitIteratorBase(T bits) : bits_(bits) { }

  Iter& operator++() {
    DCHECK_NE(bits_, 0u);
    uint32_t bit = *static_cast<Iter&>(*this);
    bits_ &= ~(static_cast<T>(1u) << bit);
    return static_cast<Iter&>(*this);
  }

  Iter& operator++(int) {
    Iter tmp(static_cast<Iter&>(*this));
    ++*this;
    return tmp;
  }

 protected:
  T bits_;

  template <typename U, typename I>
  friend bool operator==(const BitIteratorBase<U, I>& lhs, const BitIteratorBase<U, I>& rhs);
};

template <typename T, typename Iter>
bool operator==(const BitIteratorBase<T, Iter>& lhs, const BitIteratorBase<T, Iter>& rhs) {
  return lhs.bits_ == rhs.bits_;
}

template <typename T, typename Iter>
bool operator!=(const BitIteratorBase<T, Iter>& lhs, const BitIteratorBase<T, Iter>& rhs) {
  return !(lhs == rhs);
}

template <typename T>
class LowToHighBitIterator : public BitIteratorBase<T, LowToHighBitIterator<T>> {
 public:
  using BitIteratorBase<T, LowToHighBitIterator<T>>::BitIteratorBase;

  uint32_t operator*() const {
    DCHECK_NE(this->bits_, 0u);
    return CTZ(this->bits_);
  }
};

template <typename T>
class HighToLowBitIterator : public BitIteratorBase<T, HighToLowBitIterator<T>> {
 public:
  using BitIteratorBase<T, HighToLowBitIterator<T>>::BitIteratorBase;

  uint32_t operator*() const {
    DCHECK_NE(this->bits_, 0u);
    static_assert(std::numeric_limits<T>::radix == 2, "Unexpected radix!");
    return std::numeric_limits<T>::digits - 1u - CLZ(this->bits_);
  }
};

template <typename T>
IterationRange<LowToHighBitIterator<T>> LowToHighBits(T bits) {
  return IterationRange<LowToHighBitIterator<T>>(
      LowToHighBitIterator<T>(bits), LowToHighBitIterator<T>());
}

template <typename T>
IterationRange<HighToLowBitIterator<T>> HighToLowBits(T bits) {
  return IterationRange<HighToLowBitIterator<T>>(
      HighToLowBitIterator<T>(bits), HighToLowBitIterator<T>());
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_BIT_UTILS_ITERATOR_H_
