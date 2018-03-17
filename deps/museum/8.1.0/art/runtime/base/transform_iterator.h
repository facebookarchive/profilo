/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_TRANSFORM_ITERATOR_H_
#define ART_RUNTIME_BASE_TRANSFORM_ITERATOR_H_

#include <iterator>
#include <type_traits>

#include "base/iteration_range.h"

namespace art {

// The transform iterator transforms values from the base iterator with a given
// transformation function. It can serve as a replacement for std::transform(), i.e.
//    std::copy(MakeTransformIterator(begin, f), MakeTransformIterator(end, f), out)
// is equivalent to
//    std::transform(begin, end, f)
// If the function returns an l-value reference or a wrapper that supports assignment,
// the TransformIterator can be used also as an output iterator, i.e.
//    std::copy(begin, end, MakeTransformIterator(out, f))
// is equivalent to
//    for (auto it = begin; it != end; ++it) {
//      f(*out++) = *it;
//    }
template <typename BaseIterator, typename Function>
class TransformIterator {
 private:
  static_assert(std::is_base_of<
                    std::input_iterator_tag,
                    typename std::iterator_traits<BaseIterator>::iterator_category>::value,
                "Transform iterator base must be an input iterator.");

  using InputType = typename std::iterator_traits<BaseIterator>::reference;
  using ResultType = typename std::result_of<Function(InputType)>::type;

 public:
  using iterator_category = typename std::iterator_traits<BaseIterator>::iterator_category;
  using value_type =
      typename std::remove_const<typename std::remove_reference<ResultType>::type>::type;
  using difference_type = typename std::iterator_traits<BaseIterator>::difference_type;
  using pointer = typename std::conditional<
      std::is_reference<ResultType>::value,
      typename std::add_pointer<typename std::remove_reference<ResultType>::type>::type,
      TransformIterator>::type;
  using reference = ResultType;

  TransformIterator(BaseIterator base, Function fn)
      : data_(base, fn) { }

  template <typename OtherBI>
  TransformIterator(const TransformIterator<OtherBI, Function>& other)  // NOLINT, implicit
      : data_(other.base(), other.GetFunction()) {
  }

  TransformIterator& operator++() {
    ++data_.base_;
    return *this;
  }

  TransformIterator& operator++(int) {
    TransformIterator tmp(*this);
    ++*this;
    return tmp;
  }

  TransformIterator& operator--() {
    static_assert(
        std::is_base_of<std::bidirectional_iterator_tag,
                        typename std::iterator_traits<BaseIterator>::iterator_category>::value,
        "BaseIterator must be bidirectional iterator to use operator--()");
    --data_.base_;
    return *this;
  }

  TransformIterator& operator--(int) {
    TransformIterator tmp(*this);
    --*this;
    return tmp;
  }

  reference operator*() const {
    return GetFunction()(*base());
  }

  reference operator[](difference_type n) const {
    static_assert(
        std::is_base_of<std::random_access_iterator_tag,
                        typename std::iterator_traits<BaseIterator>::iterator_category>::value,
        "BaseIterator must be random access iterator to use operator[]");
    return GetFunction()(base()[n]);
  }

  TransformIterator operator+(difference_type n) const {
    static_assert(
        std::is_base_of<std::random_access_iterator_tag,
                        typename std::iterator_traits<BaseIterator>::iterator_category>::value,
        "BaseIterator must be random access iterator to use operator+");
    return TransformIterator(base() + n, GetFunction());
  }

  TransformIterator operator-(difference_type n) const {
    static_assert(
        std::is_base_of<std::random_access_iterator_tag,
                        typename std::iterator_traits<BaseIterator>::iterator_category>::value,
        "BaseIterator must be random access iterator to use operator-");
    return TransformIterator(base() - n, GetFunction());
  }

  difference_type operator-(const TransformIterator& other) const {
    static_assert(
        std::is_base_of<std::random_access_iterator_tag,
                        typename std::iterator_traits<BaseIterator>::iterator_category>::value,
        "BaseIterator must be random access iterator to use operator-");
    return base() - other.base();
  }

  // Retrieve the base iterator.
  BaseIterator base() const {
    return data_.base_;
  }

  // Retrieve the transformation function.
  const Function& GetFunction() const {
    return static_cast<const Function&>(data_);
  }

 private:
  // Allow EBO for state-less Function.
  struct Data : Function {
   public:
    Data(BaseIterator base, Function fn) : Function(fn), base_(base) { }

    BaseIterator base_;
  };

  Data data_;
};

template <typename BaseIterator1, typename BaseIterator2, typename Function>
bool operator==(const TransformIterator<BaseIterator1, Function>& lhs,
                const TransformIterator<BaseIterator2, Function>& rhs) {
  return lhs.base() == rhs.base();
}

template <typename BaseIterator1, typename BaseIterator2, typename Function>
bool operator!=(const TransformIterator<BaseIterator1, Function>& lhs,
                const TransformIterator<BaseIterator2, Function>& rhs) {
  return !(lhs == rhs);
}

template <typename BaseIterator, typename Function>
TransformIterator<BaseIterator, Function> MakeTransformIterator(BaseIterator base, Function f) {
  return TransformIterator<BaseIterator, Function>(base, f);
}

template <typename BaseRange, typename Function>
auto MakeTransformRange(BaseRange& range, Function f) {
  return MakeIterationRange(MakeTransformIterator(range.begin(), f),
                            MakeTransformIterator(range.end(), f));
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_TRANSFORM_ITERATOR_H_
