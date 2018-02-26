// Copyright 2004-present Facebook. All Rights Reserved.

#include "common.h"

#include <chrono>
#include <algorithm>

#if defined(__linux__) || defined(ANDROID)
# include <sys/syscall.h> // __NR_gettid, __NR_clock_gettime
#else
# include <pthread.h> // pthread_self()
#endif

namespace facebook {
namespace profilo {

static const int64_t kSecondNanos = 1000000000;

#if defined(__linux__) || defined(ANDROID)
int64_t monotonicTime() {
  timespec ts{};
  syscall(__NR_clock_gettime, CLOCK_MONOTONIC, &ts);
  return static_cast<int64_t>(ts.tv_sec) * kSecondNanos + ts.tv_nsec;
}
#else
int64_t monotonicTime() {
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}
#endif

#if defined(__linux__) || defined(ANDROID)
int32_t threadID() {
  return static_cast<int32_t>(syscall(__NR_gettid));
}
#elif __MACH__
int32_t threadID() {
  return static_cast<int32_t>(pthread_mach_thread_np(pthread_self()));
}
#else
#error "Do not have a threadID implementation for this platform"
#endif

#if defined(__linux__) || defined(ANDROID)
int32_t systemClockTickIntervalMs() {
  int clockTick = static_cast<int32_t>(sysconf(_SC_CLK_TCK));
  if (clockTick <= 0) {
    return 0;
  }
  return std::max(1000 / clockTick, 1);
}
#elif __MACH__
int32_t systemClockTickIntervalMs() {
  return 10; // Plain value to support tests running on OSX.
}
#else
#error "Do not have systemClockTickIntervalMs implementation for this platform"
#endif

} // profilo
} // facebook
