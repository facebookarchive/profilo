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

#include "TimerManager.h"
#include "SamplingProfiler.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <fb/log.h>
#include <random>
#include <stdexcept>

#include <profilo/util/ProcFsUtils.h>
#include <profilo/util/common.h>

namespace facebook {
namespace profilo {
namespace profiler {

namespace {

// Keep in sync with ProfiloConstants.java:
constexpr int TRACE_CONFIG_PARAM_LOGGER_PRIORITY_DEFAULT = 5;

constexpr auto kNanosecondsInSecond = 1000 * 1000 * 1000;
constexpr auto kNanosecondsInMillisecond = 1000 * 1000;

struct timespec getAbsTimeInFutureMs(int futureMs) {
  struct timespec abs_time;
  if (clock_gettime(CLOCK_REALTIME, &abs_time) == -1) {
    FBLOGE("clock_gettime %s", strerror(errno));
    abort(); // Something went wrong
  }
  long timeout_nsec = abs_time.tv_nsec + futureMs * kNanosecondsInMillisecond;
  abs_time.tv_nsec = timeout_nsec % kNanosecondsInSecond;
  abs_time.tv_sec += timeout_nsec / kNanosecondsInSecond;
  return abs_time;
}
} // namespace

void TimerManager::updateThreadTimers() {
  // Modifies state_.threadTimers
  // Must not be concurrent with stopThreadTimers()
  util::ThreadList threads;
  try {
    threads = util::threadListFromProcFs();
  } catch (const std::system_error& e) {
    // threadListFromProcFs can throw an error. Ignore it.
    return;
  }

  if (state_.whitelist != nullptr) {
    // only process whitelisted threads
    auto liveWhitelistedThreads = util::ThreadList();
    std::unique_lock<std::mutex> lock(state_.whitelist->whitelistedThreadsMtx);
    for (int32_t tid : state_.whitelist->whitelistedThreads) {
      if (threads.find(tid) != threads.end()) {
        liveWhitelistedThreads.emplace(tid);
      }
    }
    threads = std::move(liveWhitelistedThreads);
  }

  // Delete timers for threads that have died
  for (auto iter = state_.threadTimers.begin();
       iter != state_.threadTimers.end();) {
    if (threads.find(iter->first) == threads.end()) {
      iter = state_.threadTimers.erase(iter); // RAII deletes timer
    } else {
      ++iter;
    }
  }

  // Start timers for threads that are new
  for (auto& tid : threads) {
    if (state_.threadTimers.find(tid) != state_.threadTimers.end()) {
      continue;
    }
    try {
      bool ok =
          state_.threadTimers
              .emplace(
                  tid,
                  ThreadTimer(
                      tid, state_.samplingRateMs, state_.wallClockModeEnabled))
              .second;
      if (!ok) {
        FBLOGE("state_.threadTimers.insert failed");
      }
    } catch (const std::system_error& e) {
      // thread may have ended
      FBLOGV("ThreadTimer could not be created for tid %d", tid);
    }
  }
}

// must be started after sampling is enabled
void TimerManager::threadDetectLoop() {
  {
    int err = pthread_setname_np(pthread_self(), "Prflo:ThrdDetct");
    if (err) {
      FBLOGE("threadDetectLoop: pthread_setname_np: %s", strerror(err));
    }
    // https://stackoverflow.com/questions/17398075/change-native-thread-priority-on-android-in-c-c
    if (setpriority(
            PRIO_PROCESS, 0, TRACE_CONFIG_PARAM_LOGGER_PRIORITY_DEFAULT)) {
      FBLOGE("threadDetectLoop: setpriority: %s", strerror(errno));
    }
  }
  FBLOGV("ThreadDetectLoop thread %d is going into the loop...", threadID());
  int res;
  bool done;
  struct timespec nextThreadDetectWakeup = getAbsTimeInFutureMs(0);
  do {
    res = sem_timedwait(&state_.threadDetectSem, &nextThreadDetectWakeup);
    done = state_.isThreadDetectLoopDone.load();
    if (!done && res == -1 && errno == ETIMEDOUT) {
      // timed out
      nextThreadDetectWakeup =
          getAbsTimeInFutureMs(state_.threadDetectIntervalMs);
      updateThreadTimers();
      res = 0;
    }
  } while (!done && (res == 0 || errno == EINTR));
  if (res != 0) {
    FBLOGV("ThreadDetectLoop exiting with ERROR: %s", strerror(errno));
  }
  state_.threadTimers.clear();
  FBLOGV("ThreadDetectLoop thread is shutting down...");
} // namespace profiler

// --- Public API ---

TimerManager::TimerManager(
    int threadDetectIntervalMs,
    int samplingRateMs,
    bool wallClockModeEnabled,
    std::shared_ptr<Whitelist> whitelist) {
  state_.threadDetectIntervalMs = threadDetectIntervalMs;
  state_.samplingRateMs = samplingRateMs;
  state_.wallClockModeEnabled = wallClockModeEnabled;
  state_.whitelist = whitelist;

  state_.isThreadDetectLoopDone.store(false);
  if (sem_init(&state_.threadDetectSem, 0, 0)) {
    throw std::system_error(
        errno, std::system_category(), "TimerManager sem_init failed");
  }
}

void TimerManager::start() {
  // Create worker to detects new threads & starts profiling on them
  state_.threadDetectThread =
      std::thread(&TimerManager::threadDetectLoop, this);
}

void TimerManager::stop() {
  state_.isThreadDetectLoopDone.store(true);
  sem_post(&state_.threadDetectSem); // wake up
  state_.threadDetectThread.join();
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
