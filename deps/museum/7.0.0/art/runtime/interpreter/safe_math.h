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

#ifndef ART_RUNTIME_INTERPRETER_SAFE_MATH_H_
#define ART_RUNTIME_INTERPRETER_SAFE_MATH_H_

#include <functional>
#include <type_traits>

namespace art {
namespace interpreter {

// Declares a type which is the larger in bit size of the two template parameters.
template <typename T1, typename T2>
struct select_bigger {
  typedef typename std::conditional<sizeof(T1) >= sizeof(T2), T1, T2>::type type;
};

// Perform signed arithmetic Op on 'a' and 'b' with defined wrapping behavior.
template<template <typename OpT> class Op, typename T1, typename T2>
static inline typename select_bigger<T1, T2>::type SafeMath(T1 a, T2 b) {
  typedef typename select_bigger<T1, T2>::type biggest_T;
  typedef typename std::make_unsigned<biggest_T>::type unsigned_biggest_T;
  static_assert(std::is_signed<T1>::value, "Expected T1 to be signed");
  static_assert(std::is_signed<T2>::value, "Expected T2 to be signed");
  unsigned_biggest_T val1 = static_cast<unsigned_biggest_T>(static_cast<biggest_T>(a));
  unsigned_biggest_T val2 = static_cast<unsigned_biggest_T>(b);
  return static_cast<biggest_T>(Op<unsigned_biggest_T>()(val1, val2));
}

// Perform signed a signed add on 'a' and 'b' with defined wrapping behavior.
template<typename T1, typename T2>
static inline typename select_bigger<T1, T2>::type SafeAdd(T1 a, T2 b) {
  return SafeMath<std::plus>(a, b);
}

// Perform signed a signed substract on 'a' and 'b' with defined wrapping behavior.
template<typename T1, typename T2>
static inline typename select_bigger<T1, T2>::type SafeSub(T1 a, T2 b) {
  return SafeMath<std::minus>(a, b);
}

// Perform signed a signed multiply on 'a' and 'b' with defined wrapping behavior.
template<typename T1, typename T2>
static inline typename select_bigger<T1, T2>::type SafeMul(T1 a, T2 b) {
  return SafeMath<std::multiplies>(a, b);
}

}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_SAFE_MATH_H_
