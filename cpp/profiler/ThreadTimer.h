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

#pragma once

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <utility>

namespace facebook {
namespace profilo {
namespace profiler {

itimerval getInitialItimerval(int samplingRateMs);
static const timer_t INVALID_TIMER_ID =
    (timer_t)(0xdeadbeef); // type varies by OS; cannot use reinterpret_cast

class ThreadTimer {
 public:
  explicit ThreadTimer(
      int32_t tid,
      int samplingRateMs,
      bool wallClockModeEnabled);
  ~ThreadTimer();

  ThreadTimer(const ThreadTimer&) = delete;
  ThreadTimer& operator=(const ThreadTimer&) = delete;
  ThreadTimer(ThreadTimer&& other) noexcept // move constructor
      : tid_(other.tid_),
        samplingRateMs_(other.samplingRateMs_),
        wallClockModeEnabled_(other.wallClockModeEnabled_),
        timerId_(std::exchange(other.timerId_, INVALID_TIMER_ID)) {}

 private:
  int32_t tid_;
  int samplingRateMs_;
  bool wallClockModeEnabled_;
  timer_t timerId_;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
