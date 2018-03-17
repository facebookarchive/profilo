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

#ifndef ART_RUNTIME_BASE_TRANSFORM_ARRAY_REF_H_
#define ART_RUNTIME_BASE_TRANSFORM_ARRAY_REF_H_

#include <type_traits>

#include "base/array_ref.h"
#include "base/transform_iterator.h"

namespace art {

/**
 * @brief An ArrayRef<> wrapper that uses a transformation function for element access.
 */
template <typename BaseType, typename Function>
class TransformArrayRef {
 private:
  using Iter = TransformIterator<typename ArrayRef<BaseType>::iterator, Function>;

  // The Function may take a non-const reference, so const_iterator may not exist.
  using FallbackConstIter = std::iterator<std::random_access_iterator_tag, void, void, void, void>;
  using PreferredConstIter =
      TransformIterator<typename ArrayRef<BaseType>::const_iterator, Function>;
  template <typename F, typename = typename std::result_of<F(const BaseType&)>::type>
  static PreferredConstIter ConstIterHelper(int&);
  template <typename F>
  static FallbackConstIter ConstIterHelper(const int&);

  using ConstIter = decltype(ConstIterHelper<Function>(*reinterpret_cast<int*>(0)));

 public:
  using value_type = typename Iter::value_type;
  using reference = typename Iter::reference;
  using const_reference = typename ConstIter::reference;
  using pointer = typename Iter::pointer;
  using const_pointer = typename ConstIter::pointer;
  using iterator = Iter;
  using const_iterator = typename std::conditional<
      std::is_same<ConstIter, FallbackConstIter>::value,
      void,
      ConstIter>::type;
  using reverse_iterator = std::reverse_iterator<Iter>;
  using const_reverse_iterator = typename std::conditional<
      std::is_same<ConstIter, FallbackConstIter>::value,
      void,
      std::reverse_iterator<ConstIter>>::type;
  using difference_type = typename ArrayRef<BaseType>::difference_type;
  using size_type = typename ArrayRef<BaseType>::size_type;

  // Constructors.

  TransformArrayRef(const TransformArrayRef& other) = default;

  template <typename OtherBT>
  TransformArrayRef(const ArrayRef<OtherBT>& base, Function fn)
      : data_(base, fn) { }

  template <typename OtherBT,
            typename = typename std::enable_if<std::is_same<BaseType, const OtherBT>::value>::type>
  TransformArrayRef(const TransformArrayRef<OtherBT, Function>& other)  // NOLINT, implicit
      : TransformArrayRef(other.base(), other.GetFunction()) { }

  // Assignment operators.

  TransformArrayRef& operator=(const TransformArrayRef& other) = default;

  template <typename OtherBT,
            typename = typename std::enable_if<std::is_same<BaseType, const OtherBT>::value>::type>
  TransformArrayRef& operator=(const TransformArrayRef<OtherBT, Function>& other) {
    return *this = TransformArrayRef(other.base(), other.GetFunction());
  }

  // Destructor.
  ~TransformArrayRef() = default;

  // Iterators.
  iterator begin() { return MakeIterator(base().begin()); }
  const_iterator begin() const { return MakeIterator(base().cbegin()); }
  const_iterator cbegin() const { return MakeIterator(base().cbegin()); }
  iterator end() { return MakeIterator(base().end()); }
  const_iterator end() const { MakeIterator(base().cend()); }
  const_iterator cend() const { return MakeIterator(base().cend()); }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

  // Size.
  size_type size() const { return base().size(); }
  bool empty() const { return base().empty(); }

  // Element access. NOTE: Not providing data().

  reference operator[](size_type n) { return GetFunction()(base()[n]); }
  const_reference operator[](size_type n) const { return GetFunction()(base()[n]); }

  reference front() { return GetFunction()(base().front()); }
  const_reference front() const { return GetFunction()(base().front()); }

  reference back() { return GetFunction()(base().back()); }
  const_reference back() const { return GetFunction()(base().back()); }

  TransformArrayRef SubArray(size_type pos) {
    return TransformArrayRef(base().subarray(pos), GetFunction());
  }
  TransformArrayRef SubArray(size_type pos) const {
    return TransformArrayRef(base().subarray(pos), GetFunction());
  }
  TransformArrayRef SubArray(size_type pos, size_type length) const {
    return TransformArrayRef(base().subarray(pos, length), GetFunction());
  }

  // Retrieve the base ArrayRef<>.
  ArrayRef<BaseType> base() {
    return data_.base_;
  }
  ArrayRef<const BaseType> base() const {
    return ArrayRef<const BaseType>(data_.base_);
  }

 private:
  // Allow EBO for state-less Function.
  struct Data : Function {
   public:
    Data(ArrayRef<BaseType> base, Function fn) : Function(fn), base_(base) { }

    ArrayRef<BaseType> base_;
  };

  const Function& GetFunction() const {
    return static_cast<const Function&>(data_);
  }

  template <typename BaseIterator>
  auto MakeIterator(BaseIterator base) const {
    return MakeTransformIterator(base, GetFunction());
  }

  Data data_;

  template <typename OtherBT, typename OtherFunction>
  friend class TransformArrayRef;
};

template <typename BaseType, typename Function>
bool operator==(const TransformArrayRef<BaseType, Function>& lhs,
                const TransformArrayRef<BaseType, Function>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename BaseType, typename Function>
bool operator!=(const TransformArrayRef<BaseType, Function>& lhs,
                const TransformArrayRef<BaseType, Function>& rhs) {
  return !(lhs == rhs);
}

template <typename ValueType, typename Function>
TransformArrayRef<ValueType, Function> MakeTransformArrayRef(
    ArrayRef<ValueType> container, Function f) {
  return TransformArrayRef<ValueType, Function>(container, f);
}

template <typename Container, typename Function>
TransformArrayRef<typename Container::value_type, Function> MakeTransformArrayRef(
    Container& container, Function f) {
  return TransformArrayRef<typename Container::value_type, Function>(
      ArrayRef<typename Container::value_type>(container.data(), container.size()), f);
}

template <typename Container, typename Function>
TransformArrayRef<const typename Container::value_type, Function> MakeTransformArrayRef(
    const Container& container, Function f) {
  return TransformArrayRef<const typename Container::value_type, Function>(
      ArrayRef<const typename Container::value_type>(container.data(), container.size()), f);
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_TRANSFORM_ARRAY_REF_H_
