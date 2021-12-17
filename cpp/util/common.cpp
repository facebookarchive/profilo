/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common.h"

#include <dlfcn.h>
#include <fb/log.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <system_error>

#if defined(__linux__) || defined(ANDROID)
#include <sys/syscall.h> // __NR_gettid, __NR_clock_gettime
#endif

namespace facebook {
namespace profilo {

#if defined(__linux__) || defined(ANDROID)
static const int64_t kSecondNanos = 1000000000;

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

#ifdef ANDROID
typedef pid_t (*gettid_t)(pthread_t);
// Returns any available bionic helper to get the tid from the
// pthread_t. This helps us avoid a syscall on a pretty hot paths.
static gettid_t getBionicGetTid() {
  // This is fast because libc is always loaded.
  auto handle = dlopen("libc.so", RTLD_NOW | RTLD_GLOBAL);

  if (!handle) {
    FBLOGV("couldn't open libc: %s", dlerror());
    return nullptr;
  }

  // L+ symbol name
  gettid_t result = (gettid_t)dlsym(handle, "pthread_gettid_np");
  FBLOGV("Found pthread_gettid_np: %p", result);
  if (!result) {
    // Pre-L symbol name. This should be available all the way down to ICS
    // but be extra super cautious.
    result = (gettid_t)dlsym(handle, "__pthread_gettid");
    FBLOGV("__pthread_gettid: %p", result);
  }

  dlclose(handle);
  return result;
}
#endif

#ifdef __linux__
int32_t threadID() {
#ifdef ANDROID
  static gettid_t bionicPthreadGetTid = getBionicGetTid();

  if (bionicPthreadGetTid) {
    return bionicPthreadGetTid(pthread_self());
  }
#endif
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

#if defined(__linux__) || defined(ANDROID)
int32_t cpuClockResolutionMicros() {
  timespec clock_res_timespec;
  // It was empirically determined that this clock resolution is equal to actual
  // size of kernel jiffy.
  auto res = clock_getres(CLOCK_REALTIME_COARSE, &clock_res_timespec);
  if (res != 0) {
    return -1;
  }
  return clock_res_timespec.tv_nsec / 1000;
}
#else
int32_t cpuClockResolutionMicros() {
  return 10000;
}
#endif

#if defined(ANDROID)
#include <sys/system_properties.h>

std::string get_system_property(const char* key) {
  char prop_value[PROP_VALUE_MAX]{};
  if (__system_property_get(key, prop_value) > 0) {
    return std::string(prop_value);
  }
  return "";
}
#else
std::string get_system_property(const char* _) {
  return "";
}
#endif

// Creates the directory specified by a path, creating intermediate
// directories as needed
void mkdirs(char const* dir) {
  auto len = strlen(dir);
  char partial[len + 1];
  memset(partial, 0, len + 1);
  strncpy(partial, dir, len);
  struct stat s {};
  char* delim{};

  // Iterate our path backwards until we find a string we can stat(), which
  // is an indicator of an existent directory
  while (stat(partial, &s)) {
    delim = strrchr(partial, '/');
    *delim = 0;
  }

  // <partial> now contains a path to a directory that actually exists, so
  // create all the intermediate directories until we finally have the
  // file system hierarchy specified by <dir>
  while (delim != nullptr && delim < partial + len) {
    *delim = '/';
    delim = strchr(delim + 1, '\0');
    if (mkdirat(0, partial, S_IRWXU | S_IRWXG)) {
      if (errno != EEXIST) {
        std::stringstream ss;
        ss << "Could not mkdirat() folder ";
        ss << partial;
        ss << ", errno = ";
        ss << strerror(errno);
        throw std::system_error(errno, std::system_category(), ss.str());
      }
    }
  }
}

uint64_t parse_ull(char* str, char** end) {
  static constexpr int kMaxDigits = 20;

  char* cur = str;
  while (*cur == ' ') {
    ++cur;
  }

  uint64_t result = 0;
  uint8_t len = 0;
  while (*cur >= '0' && *cur <= '9' && len <= kMaxDigits) {
    result *= 10;
    result += (*cur - '0');
    ++len;
    ++cur;
  }

  *end = cur;
  return result;
}

} // namespace profilo
} // namespace facebook
