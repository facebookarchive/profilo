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

#include <counters/ProcFs.h>
#include <profilo/MultiBufferLogger.h>
#include <mutex>

using facebook::profilo::logger::MultiBufferLogger;

namespace facebook {
namespace profilo {
namespace counters {

namespace {

constexpr auto kAllThreadsStatsMask = StatType::CPU_TIME |
    StatType::MAJOR_FAULTS | StatType::MINOR_FAULTS |
    StatType::KERNEL_CPU_TIME | StatType::THREAD_PRIORITY;

constexpr auto kHighFreqStatsMask = StatType::CPU_TIME | StatType::STATE |
    StatType::MAJOR_FAULTS | StatType::CPU_NUM | StatType::THREAD_PRIORITY |
    StatType::HIGH_PRECISION_CPU_TIME | StatType::WAIT_TO_RUN_TIME |
    StatType::NR_VOLUNTARY_SWITCHES | StatType::NR_INVOLUNTARY_SWITCHES |
    StatType::IOWAIT_SUM | StatType::IOWAIT_COUNT;

} // namespace

class ThreadCounters {
 public:
  ThreadCounters(MultiBufferLogger& logger) : cache_(logger) {}

  void logCounters(
      bool highFrequencyMode,
      std::unordered_set<int32_t>& ignoredTids) {
    std::lock_guard<std::mutex> lock(mtx_);
    cache_.sampleAndLogForEach(
        kAllThreadsStatsMask, highFrequencyMode ? &ignoredTids : nullptr);
  }

  int32_t getAvailableCounters() {
    std::lock_guard<std::mutex> lock(mtx_);
    return cache_.getStatsAvailabililty(getpid()) | extraAvailableCounters_;
  }

  void logHighFreqCounters(std::unordered_set<int32_t>& tids) {
    // Whitelist thread profiling mode
    std::lock_guard<std::mutex> lockC(mtx_);
    for (int32_t tid : tids) {
      cache_.sampleAndLogForThread(tid, kHighFreqStatsMask);
    }
  }

 private:
  int32_t extraAvailableCounters_;
  std::mutex mtx_; // Guards cache_
  ThreadCache cache_;
};

} // namespace counters
} // namespace profilo
} // namespace facebook
