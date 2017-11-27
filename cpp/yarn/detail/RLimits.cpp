// Copyright 2004-present Facebook. All Rights Reserved.

#include <yarn/detail/RLimits.h>

namespace facebook {
namespace yarn {
namespace detail {

rlimit getrlimit(int resource) {
  rlimit res{};
  if (getrlimit(resource, &res) != 0) {
    throw std::system_error(errno, std::system_category());
  }
  return res;
}

void setrlimit(int resource, const rlimit& limits) {
  if (setrlimit(resource, &limits) != 0) {
    throw std::system_error(errno, std::system_category());
  }
}

} // detail
} // yarn
} // facebook
