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

#include "ThreadTimer.h"

#include <semaphore.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace facebook {
namespace profilo {
namespace profiler {

struct Whitelist;

struct TimerManagerState {
  int threadDetectIntervalMs;
  int samplingRateMs;
  bool wallClockModeEnabled;

  // whitelist is optional; use null for "all threads"
  std::shared_ptr<Whitelist> whitelist;

  std::thread threadDetectThread;
  sem_t threadDetectSem;
  std::atomic_bool isThreadDetectLoopDone;
  std::unordered_map<pid_t, ThreadTimer> threadTimers;
};

class TimerManager {
 public:
  explicit TimerManager(
      int threadDetectIntervalMs,
      int samplingRateMs,
      bool wallClockModeEnabled,
      std::shared_ptr<Whitelist> whitelist);
  ~TimerManager() = default;
  void start(); // potentially blocks
  void stop(); // potentially blocks

 private:
  TimerManagerState state_;
  void updateThreadTimers();
  void threadDetectLoop();
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
