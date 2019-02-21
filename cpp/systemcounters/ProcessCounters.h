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

using facebook::profilo::util::StatType;

namespace facebook {
namespace profilo {

// Separating process counters from thread counters
template <typename TaskStatFile, typename TaskSchedFile, typename Logger>
class ProcessCounters {
 public:
  ProcessCounters() = default;
  // For Tests
  ProcessCounters(
      std::unique_ptr<TaskStatFile> taskStatFile,
      std::unique_ptr<TaskSchedFile> schedStatFile)
      : processStatFile_(std::move(taskStatFile)),
        schedStats_(std::move(schedStatFile)),
        schedStatsTracingDisabled_(false),
        extraAvailableCounters_(false) {}

  void logCounters() {
    logProcessCounters();
    logProcessSchedCounters();
  }
  int32_t getAvailableCounters() {
    return extraAvailableCounters_;
  }

 private:
  void logProcessCounters() {
    if (!processStatFile_) {
      processStatFile_.reset(new TaskStatFile("/proc/self/stat"));
    }

    auto prevInfo = processStatFile_->getInfo();
    util::TaskStatInfo currInfo;

    try {
      currInfo = processStatFile_->refresh();
    } catch (const std::system_error& e) {
      return;
    } catch (const std::runtime_error& e) {
      return;
    }

    auto time = monotonicTime();
    auto tid = threadID();
    Logger& logger = Logger::get();

    if (prevInfo.cpuTime != 0) {
      logMonotonicCounter<Logger>(
          prevInfo.cpuTime,
          currInfo.cpuTime,
          tid,
          time,
          QuickLogConstants::PROC_CPU_TIME,
          logger);

      logMonotonicCounter<Logger>(
          prevInfo.kernelCpuTimeMs,
          currInfo.kernelCpuTimeMs,
          tid,
          time,
          QuickLogConstants::PROC_KERNEL_CPU_TIME,
          logger);
    }

    logMonotonicCounter<Logger>(
        prevInfo.majorFaults,
        currInfo.majorFaults,
        tid,
        time,
        QuickLogConstants::PROC_SW_FAULTS_MAJOR,
        logger);

    logMonotonicCounter<Logger>(
        prevInfo.minorFaults,
        currInfo.minorFaults,
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
    util::SchedInfo currInfo;
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

  std::unique_ptr<TaskStatFile> processStatFile_;
  std::unique_ptr<TaskSchedFile> schedStats_;
  bool schedStatsTracingDisabled_;
  int32_t extraAvailableCounters_;
};

} // namespace profilo
} // namespace facebook
