// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>

namespace facebook {
namespace yarn {
namespace detail {

// Array specialization
template<typename T>
typename std::enable_if<
  std::is_array<T>::value, 
  std::unique_ptr<T>>::type make_unique(size_t size)
{
  static_assert(
    std::extent<T>::value == 0,
    "Cannot use make_unique<T[N]>, use make_unique<T[]> instead"
  );
  typedef typename std::remove_extent<T>::type U;
  return std::unique_ptr<T>(new U[size]);
}


template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // detail
} // yarn
} // facebook
