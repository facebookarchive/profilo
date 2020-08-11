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

#include <sys/resource.h>
#include <sys/time.h>

#include <counters/ProcFs.h>
#include <util/common.h>
#include "common.h"

namespace facebook {
namespace profilo {
namespace counters {

namespace {

static constexpr auto kMillisInSec = 1000;
static constexpr auto kMicrosInMillis = 1000;

inline uint64_t timeval_to_millis(timeval& tv) {
  return tv.tv_sec * kMillisInSec + tv.tv_usec / kMicrosInMillis;
}

inline uint64_t timeval_sum_to_millis(timeval& tv1, timeval& tv2) {
  return (tv1.tv_sec + tv2.tv_sec) * kMillisInSec +
      (tv1.tv_usec + tv2.tv_usec) / kMicrosInMillis;
}

struct DefaultGetRusageStatsProvider {
  DefaultGetRusageStatsProvider() = default;

  void refresh() {
    prevStats = curStats;
    getrusage(RUSAGE_SELF, &curStats);
  }

  rusage prevStats;
  rusage curStats;
};
} // namespace

// Separating process counters from thread counters
template <
    typename TaskSchedFile,
    typename Logger,
    typename GetRusageStats = DefaultGetRusageStatsProvider,
    typename StatmFile = ProcStatmFile>
class ProcessCounters {
 public:
  void logCounters() {
    logProcessCounters();
    logProcessSchedCounters();
    logProcessStatmCounters();
  }

  int32_t getAvailableCounters() {
    return extraAvailableCounters_;
  }

 private:
  void logProcessCounters() {
    auto time = monotonicTime();
    auto tid = threadID();
    Logger& logger = Logger::get();
    getRusageStats_.refresh();

    rusage& prev = getRusageStats_.prevStats;
    rusage& cur = getRusageStats_.curStats;

    logMonotonicCounter<Logger>(
        timeval_sum_to_millis(prev.ru_utime, prev.ru_stime),
        timeval_sum_to_millis(cur.ru_utime, cur.ru_stime),
        tid,
        time,
        QuickLogConstants::PROC_CPU_TIME,
        logger);

    logMonotonicCounter<Logger>(
        timeval_to_millis(prev.ru_stime),
        timeval_to_millis(cur.ru_stime),
        tid,
        time,
        QuickLogConstants::PROC_KERNEL_CPU_TIME,
        logger);

    logMonotonicCounter<Logger>(
        prev.ru_majflt,
        cur.ru_majflt,
        tid,
        time,
        QuickLogConstants::PROC_SW_FAULTS_MAJOR,
        logger);

    logMonotonicCounter<Logger>(
        prev.ru_minflt,
        cur.ru_minflt,
        tid,
        time,
        QuickLogConstants::PROC_SW_FAULTS_MINOR,
        logger);
  }

  void logProcessSchedCounters() {
    if (schedStatsTracingDisabled_) {
      return;
    }
    if (!schedStats_) {
      schedStats_.reset(new TaskSchedFile("/proc/self/sched"));
    }

    auto prevInfo = schedStats_->getInfo();
    SchedInfo currInfo;
    try {
      currInfo = schedStats_->refresh();
      extraAvailableCounters_ |= schedStats_->availableStatsMask;
      if (schedStats_->availableStatsMask == 0) {
        schedStatsTracingDisabled_ = true;
        schedStats_.reset(nullptr);
        return;
      }
    } catch (...) {
      schedStatsTracingDisabled_ = true;
      schedStats_.reset(nullptr);
      return;
    }

    auto time = monotonicTime();
    auto tid = threadID();
    Logger& logger = Logger::get();

    if (schedStats_->availableStatsMask & StatType::IOWAIT_SUM) {
      logMonotonicCounter<Logger>(
          prevInfo.iowaitSum,
          currInfo.iowaitSum,
          tid,
          time,
          QuickLogConstants::PROC_IOWAIT_TIME,
          logger);
    }
    if (schedStats_->availableStatsMask & StatType::IOWAIT_COUNT) {
      logMonotonicCounter<Logger>(
          prevInfo.iowaitCount,
          currInfo.iowaitCount,
          tid,
          time,
          QuickLogConstants::PROC_IOWAIT_COUNT,
          logger);
    }
    if (schedStats_->availableStatsMask & StatType::NR_VOLUNTARY_SWITCHES) {
      logMonotonicCounter<Logger>(
          prevInfo.nrVoluntarySwitches,
          currInfo.nrVoluntarySwitches,
          tid,
          time,
          QuickLogConstants::PROC_CONTEXT_SWITCHES_VOLUNTARY,
          logger);
    }
    if (schedStats_->availableStatsMask & StatType::NR_INVOLUNTARY_SWITCHES) {
      logMonotonicCounter<Logger>(
          prevInfo.nrInvoluntarySwitches,
          currInfo.nrInvoluntarySwitches,
          tid,
          time,
          QuickLogConstants::PROC_CONTEXT_SWITCHES_INVOLUNTARY,
          logger);
    }
  }

  void logProcessStatmCounters() {
    if (!statmStats_) {
      statmStats_.reset(new StatmFile());
    }

    StatmInfo prevInfo = statmStats_->getInfo();
    StatmInfo currInfo = statmStats_->refresh();

    auto tid = threadID();
    auto time = monotonicTime();
    Logger& logger = Logger::get();

    logNonMonotonicCounter(
        prevInfo.resident,
        currInfo.resident,
        tid,
        time,
        QuickLogConstants::PROC_STATM_RESIDENT,
        logger);
    logNonMonotonicCounter(
        prevInfo.shared,
        currInfo.shared,
        tid,
        time,
        QuickLogConstants::PROC_STATM_SHARED,
        logger);
  }

  std::unique_ptr<TaskSchedFile> schedStats_;
  bool schedStatsTracingDisabled_;
  int32_t extraAvailableCounters_;
  GetRusageStats getRusageStats_;
  std::unique_ptr<StatmFile> statmStats_;

  friend class ProcessCountersTestAccessor;
};

} // namespace counters
} // namespace profilo
} // namespace facebook
