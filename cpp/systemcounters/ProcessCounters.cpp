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

#include "ProcessCounters.h"

#include <sys/resource.h>
#include <sys/time.h>
#include <util/common.h>

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

} // namespace

void ProcessCounters::logCounters() {
  auto time = monotonicTime();

  logProcessCounters(time);
  logProcessSchedCounters(time);
  logProcessStatmCounters(time);
}

void ProcessCounters::logProcessCounters(int64_t time) {
  rusage rusageStats;
  getrusage(RUSAGE_SELF, &rusageStats);
  stats_.cpuTimeMs.record(
      timeval_sum_to_millis(rusageStats.ru_utime, rusageStats.ru_stime), time);
  stats_.kernelCpuTimeMs.record(timeval_to_millis(rusageStats.ru_stime), time);
  stats_.majorFaults.record(rusageStats.ru_majflt, time);
  stats_.minorFaults.record(rusageStats.ru_minflt, time);
}

void ProcessCounters::logProcessSchedCounters(int64_t time) {
  if (schedStatsTracingDisabled_) {
    return;
  }
  if (!schedStats_) {
    schedStats_.reset(new TaskSchedFile("/proc/self/sched"));
  }

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

  stats_.iowaitSum.record(currInfo.iowaitSum, time);
  stats_.iowaitCount.record(currInfo.iowaitCount, time);
  stats_.nrVoluntarySwitches.record(currInfo.nrVoluntarySwitches, time);
  stats_.nrInvoluntarySwitches.record(currInfo.nrInvoluntarySwitches, time);
}

void ProcessCounters::logProcessStatmCounters(int64_t time) {
  if (!statmStats_) {
    statmStats_.reset(new ProcStatmFile());
  }
  StatmInfo currInfo = statmStats_->refresh();
  stats_.memResident.record(currInfo.resident, time);
  stats_.memShared.record(currInfo.shared, time);
}

} // namespace counters
} // namespace profilo
} // namespace facebook
