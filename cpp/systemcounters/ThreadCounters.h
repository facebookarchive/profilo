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

#include <util/ProcFs.h>
#include "common.h"

#include <mutex>

using facebook::profilo::util::StatType;

namespace facebook {
namespace profilo {

namespace {

constexpr auto kAllThreadsStatsMask = StatType::CPU_TIME | StatType::STATE |
    StatType::MAJOR_FAULTS | StatType::MINOR_FAULTS | StatType::KERNEL_CPU_TIME;

constexpr auto kHighFreqStatsMask = StatType::CPU_TIME | StatType::STATE |
    StatType::MAJOR_FAULTS | StatType::CPU_NUM |
    StatType::HIGH_PRECISION_CPU_TIME | StatType::WAIT_TO_RUN_TIME |
    StatType::NR_VOLUNTARY_SWITCHES | StatType::NR_INVOLUNTARY_SWITCHES |
    StatType::IOWAIT_SUM | StatType::IOWAIT_COUNT;

} // namespace

template <typename ThreadCache, typename Logger>
class ThreadCounters {
 private:
  int32_t extraAvailableCounters_;
  std::mutex mtx_; // Guards cache_
  ThreadCache cache_;

  static void threadCountersCallback(
      uint32_t tid,
      util::ThreadStatInfo& prevInfo,
      util::ThreadStatInfo& currInfo) {
    Logger& logger = Logger::get();

    if (prevInfo.highPrecisionCpuTimeMs != 0 &&
        (currInfo.availableStatsMask & StatType::HIGH_PRECISION_CPU_TIME)) {
      // Don't log the initial value
      logMonotonicCounter<Logger>(
          prevInfo.highPrecisionCpuTimeMs,
          currInfo.highPrecisionCpuTimeMs,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::THREAD_CPU_TIME,
          logger,
          (prevInfo.statChangeMask & StatType::HIGH_PRECISION_CPU_TIME) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    } else if (
        prevInfo.cpuTimeMs != 0 &&
        (currInfo.availableStatsMask & StatType::CPU_TIME)) {
      // Don't log the initial value
      logMonotonicCounter<Logger>(
          prevInfo.cpuTimeMs,
          currInfo.cpuTimeMs,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::THREAD_CPU_TIME,
          logger,
          (prevInfo.statChangeMask & StatType::CPU_TIME) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (prevInfo.waitToRunTimeMs != 0 &&
        (currInfo.availableStatsMask & StatType::WAIT_TO_RUN_TIME)) {
      logMonotonicCounter<Logger>(
          prevInfo.waitToRunTimeMs,
          currInfo.waitToRunTimeMs,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::THREAD_WAIT_IN_RUNQUEUE_TIME,
          logger,
          (prevInfo.statChangeMask & StatType::WAIT_TO_RUN_TIME) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::MAJOR_FAULTS) {
      logMonotonicCounter<Logger>(
          prevInfo.majorFaults,
          currInfo.majorFaults,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::QL_THREAD_FAULTS_MAJOR,
          logger,
          (prevInfo.statChangeMask & StatType::MAJOR_FAULTS) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::NR_VOLUNTARY_SWITCHES) {
      logMonotonicCounter<Logger>(
          prevInfo.nrVoluntarySwitches,
          currInfo.nrVoluntarySwitches,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::CONTEXT_SWITCHES_VOLUNTARY,
          logger,
          (prevInfo.statChangeMask & StatType::NR_VOLUNTARY_SWITCHES) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::NR_INVOLUNTARY_SWITCHES) {
      logMonotonicCounter<Logger>(
          prevInfo.nrInvoluntarySwitches,
          currInfo.nrInvoluntarySwitches,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::CONTEXT_SWITCHES_INVOLUNTARY,
          logger,
          (prevInfo.statChangeMask & StatType::NR_INVOLUNTARY_SWITCHES) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::IOWAIT_SUM) {
      logMonotonicCounter<Logger>(
          prevInfo.iowaitSum,
          currInfo.iowaitSum,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::IOWAIT_TIME,
          logger,
          (prevInfo.statChangeMask & StatType::IOWAIT_SUM) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::IOWAIT_COUNT) {
      logMonotonicCounter<Logger>(
          prevInfo.iowaitCount,
          currInfo.iowaitCount,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::IOWAIT_COUNT,
          logger,
          (prevInfo.statChangeMask & StatType::IOWAIT_COUNT) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::CPU_NUM) {
      logNonMonotonicCounter<Logger>(
          prevInfo.cpuNum,
          currInfo.cpuNum,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::THREAD_CPU_NUM,
          logger);
    }
    if (currInfo.availableStatsMask & StatType::KERNEL_CPU_TIME) {
      logMonotonicCounter<Logger>(
          prevInfo.kernelCpuTimeMs,
          currInfo.kernelCpuTimeMs,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::THREAD_KERNEL_CPU_TIME,
          logger,
          (prevInfo.statChangeMask & StatType::KERNEL_CPU_TIME) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
    if (currInfo.availableStatsMask & StatType::MINOR_FAULTS) {
      logMonotonicCounter<Logger>(
          prevInfo.minorFaults,
          currInfo.minorFaults,
          tid,
          currInfo.monotonicStatTime,
          QuickLogConstants::THREAD_SW_FAULTS_MINOR,
          logger,
          (prevInfo.statChangeMask & StatType::MINOR_FAULTS) == 0
              ? prevInfo.monotonicStatTime
              : 0);
    }
  }

 public:
  ThreadCounters() = default;
  // For Tests
  ThreadCounters(ThreadCache cache) : cache_(cache) {}

  void logCounters(
      bool highFrequencyMode,
      std::unordered_set<int32_t>& ignoredTids) {
    std::lock_guard<std::mutex> lock(mtx_);
    cache_.forEach(
        &threadCountersCallback,
        kAllThreadsStatsMask,
        highFrequencyMode ? &ignoredTids : nullptr);
  }

  int32_t getAvailableCounters() {
    std::lock_guard<std::mutex> lock(mtx_);
    return cache_.getStatsAvailabililty(getpid()) | extraAvailableCounters_;
  }

  void logHighFreqCounters(std::unordered_set<int32_t>& tids) {
    // Whitelist thread profiling mode
    std::lock_guard<std::mutex> lockC(mtx_);
    for (int32_t tid : tids) {
      cache_.forThread(
          tid, &ThreadCounters::threadCountersCallback, kHighFreqStatsMask);
    }
  }
};

} // namespace profilo
} // namespace facebook
