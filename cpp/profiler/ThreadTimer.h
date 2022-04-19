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

#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <utility>

/**
 * Profiling real time signal number.
 */
#define PROFILER_SIGNAL SIGURG

namespace facebook {
namespace profilo {
namespace profiler {

itimerval getInitialItimerval(int samplingRateMs);
static const timer_t INVALID_TIMER_ID =
    (timer_t)(0xdeadbeef); // type varies by OS; cannot use reinterpret_cast

class ThreadTimer {
 public:
  enum class Type {
    CpuTime = 1,
    WallTime,
  };

  explicit ThreadTimer(int32_t tid, int samplingRateMs, Type timerType);
  ~ThreadTimer();

  ThreadTimer(const ThreadTimer&) = delete;
  ThreadTimer& operator=(const ThreadTimer&) = delete;
  ThreadTimer(ThreadTimer&& other) noexcept // move constructor
      : tid_(other.tid_),
        samplingRateMs_(other.samplingRateMs_),
        timerType_(other.timerType_),
        timerId_(std::exchange(other.timerId_, INVALID_TIMER_ID)) {}

  static Type decodeType(long salted);

  static long encodeType(Type type);

 private:
  int32_t tid_;
  int samplingRateMs_;
  Type timerType_;
  timer_t timerId_;
  static long typeSeed;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
