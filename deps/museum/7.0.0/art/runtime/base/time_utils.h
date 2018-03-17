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

#ifndef ART_RUNTIME_BASE_TIME_UTILS_H_
#define ART_RUNTIME_BASE_TIME_UTILS_H_

#include <stdint.h>
#include <string>
#include <time.h>

#include "base/macros.h"

namespace art {

enum TimeUnit {
  kTimeUnitNanosecond,
  kTimeUnitMicrosecond,
  kTimeUnitMillisecond,
  kTimeUnitSecond,
};

// Returns a human-readable time string which prints every nanosecond while trying to limit the
// number of trailing zeros. Prints using the largest human readable unit up to a second.
// e.g. "1ms", "1.000000001s", "1.001us"
std::string PrettyDuration(uint64_t nano_duration, size_t max_fraction_digits = 3);

// Format a nanosecond time to specified units.
std::string FormatDuration(uint64_t nano_duration, TimeUnit time_unit,
                           size_t max_fraction_digits);

// Get the appropriate unit for a nanosecond duration.
TimeUnit GetAppropriateTimeUnit(uint64_t nano_duration);

// Get the divisor to convert from a nanoseconds to a time unit.
uint64_t GetNsToTimeUnitDivisor(TimeUnit time_unit);

// Returns the current date in ISO yyyy-mm-dd hh:mm:ss format.
std::string GetIsoDate();

// Returns the monotonic time since some unspecified starting point in milliseconds.
uint64_t MilliTime();

// Returns the monotonic time since some unspecified starting point in microseconds.
uint64_t MicroTime();

// Returns the monotonic time since some unspecified starting point in nanoseconds.
uint64_t NanoTime();

// Returns the thread-specific CPU-time clock in nanoseconds or -1 if unavailable.
uint64_t ThreadCpuNanoTime();

// Converts the given number of nanoseconds to milliseconds.
static constexpr inline uint64_t NsToMs(uint64_t ns) {
  return ns / 1000 / 1000;
}

// Converts the given number of milliseconds to nanoseconds
static constexpr inline uint64_t MsToNs(uint64_t ms) {
  return ms * 1000 * 1000;
}

#if defined(__APPLE__)
// No clocks to specify on OS/X, fake value to pass to routines that require a clock.
#define CLOCK_REALTIME 0xebadf00d
#endif

// Sleep for the given number of nanoseconds, a bad way to handle contention.
void NanoSleep(uint64_t ns);

// Initialize a timespec to either a relative time (ms,ns), or to the absolute
// time corresponding to the indicated clock value plus the supplied offset.
void InitTimeSpec(bool absolute, int clock, int64_t ms, int32_t ns, timespec* ts);

}  // namespace art

#endif  // ART_RUNTIME_BASE_TIME_UTILS_H_
