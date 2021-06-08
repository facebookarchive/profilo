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

#include <profilo/MultiBufferLogger.h>
#include <profilo/counters/Counter.h>
#include <profilo/counters/ProcFs.h>
#include <profilo/systemcounters/MappingAggregator.h>
#include <unistd.h>

using facebook::profilo::logger::MultiBufferLogger;

namespace facebook {
namespace profilo {
namespace counters {

struct ProcessStats {
  TraceCounter cpuTimeMs;
  TraceCounter kernelCpuTimeMs;
  TraceCounter majorFaults;
  TraceCounter minorFaults;
  TraceCounter nrVoluntarySwitches;
  TraceCounter nrInvoluntarySwitches;
  TraceCounter iowaitSum;
  TraceCounter iowaitCount;
  TraceCounter memResident;
  TraceCounter memShared;
  TraceCounter glDev;
  TraceCounter dmabuf;
};

class ProcessCounters {
 public:
  ProcessCounters(MultiBufferLogger& logger, int32_t pid = getpid())
      : schedStats_(),
        schedStatsTracingDisabled_(false),
        extraAvailableCounters_(0),
        statmStats_(),
        mappingAggregator_(),
        stats_(ProcessStats{
            .cpuTimeMs =
                TraceCounter(logger, QuickLogConstants::PROC_CPU_TIME, pid),
            .kernelCpuTimeMs = TraceCounter(
                logger,
                QuickLogConstants::PROC_KERNEL_CPU_TIME,
                pid),
            .majorFaults = TraceCounter(
                logger,
                QuickLogConstants::PROC_SW_FAULTS_MAJOR,
                pid),
            .minorFaults = TraceCounter(
                logger,
                QuickLogConstants::PROC_SW_FAULTS_MINOR,
                pid),
            .nrVoluntarySwitches = TraceCounter(
                logger,
                QuickLogConstants::PROC_CONTEXT_SWITCHES_VOLUNTARY,
                pid),
            .nrInvoluntarySwitches = TraceCounter(
                logger,
                QuickLogConstants::PROC_CONTEXT_SWITCHES_INVOLUNTARY,
                pid),
            .iowaitSum =
                TraceCounter(logger, QuickLogConstants::PROC_IOWAIT_TIME, pid),
            .iowaitCount =
                TraceCounter(logger, QuickLogConstants::PROC_IOWAIT_COUNT, pid),
            .memResident = TraceCounter(
                logger,
                QuickLogConstants::PROC_STATM_RESIDENT,
                pid),
            .memShared =
                TraceCounter(logger, QuickLogConstants::PROC_STATM_SHARED, pid),
            .glDev =
                TraceCounter(logger, QuickLogConstants::MAPPING_GL_DEV, pid),
            .dmabuf =
                TraceCounter(logger, QuickLogConstants::MAPPING_DMABUF, pid),
        }) {}

  void logCounters();

  int32_t getAvailableCounters() {
    return extraAvailableCounters_;
  }

 private:
  void logProcessCounters(int64_t time);

  void logMappingCounters(int64_t time);

  void logProcessSchedCounters(int64_t time);

  void logProcessStatmCounters(int64_t time);

  std::unique_ptr<TaskSchedFile> schedStats_;
  bool schedStatsTracingDisabled_;
  int32_t extraAvailableCounters_;
  std::unique_ptr<ProcStatmFile> statmStats_;
  MappingAggregator mappingAggregator_;
  ProcessStats stats_;
};

} // namespace counters
} // namespace profilo
} // namespace facebook
