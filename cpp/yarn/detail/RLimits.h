// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>
#include <system_error>
#include <sys/resource.h>

namespace facebook {
namespace yarn {
namespace detail {

rlimit getrlimit(int resource);
void setrlimit(int resource, const rlimit& limits);

} // detail
} // yarn
} // facebook
