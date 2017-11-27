// Copyright 2004-present Facebook. All Rights Reserved.

#include <atomic>
#include <type_traits>
#include <unistd.h>

namespace facebook {
namespace loom {

int64_t monotonicTime();
int32_t threadID();
// Returns 0 if value was not found, and 1 if value <= 1, actual value otherwise
int32_t systemClockTickIntervalMs();

} // loom
} // facebook
