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

#include "SystemCounterThread.h"
#include <profilo/Logger.h>
#include <profilo/LogEntry.h>
#include <profilo/log.h>
#include <fb/log.h>
#include <util/common.h>

#include <sys/sysinfo.h>
#include <sys/syscall.h>

#include <malloc.h>

using facebook::jni::alias_ref;
using facebook::jni::local_ref;

namespace facebook {
namespace profilo {

namespace {

constexpr auto kTidAllThreads = 0;

const auto kAllThreadsStatsMask = util::kFileStats[util::StatFileType::STAT];
const auto kHighFreqStatsMask =
  util::kFileStats[util::StatFileType::SCHEDSTAT] |
    util::kFileStats[util::StatFileType::SCHED];

inline void logCounter(
    Logger& logger,
    int32_t counter_name,
    int64_t value,
    int32_t thread_id = threadID()) {
  logger.write(StandardEntry {
    .id = 0,
    .type = entries::COUNTER,
    .timestamp = monotonicTime(),
    .tid = thread_id,
    .callid = counter_name,
    .matchid = 0,
    .extra = value,
  });
}

inline void logMonotonicCounter(
  long prev,
  long curr,
  int tid,
  uint32_t quicklog_id,
  int threshold = 0) {
  auto& logger = Logger::get();

  if (curr > prev + threshold) {
    logCounter(logger, quicklog_id, curr, tid);
  }
}

inline void logCPUTimeCounter(long prevCpuTime, long currCpuTime, int tid) {
  const auto kThresholdMs = 1;
  logMonotonicCounter(
    prevCpuTime,
    currCpuTime,
    tid,
    QuickLogConstants::THREAD_CPU_TIME,
    kThresholdMs);
}

inline void logThreadMajorFaults(
  long prev_mfaults,
  long curr_mfaults,
  int tid) {
  logMonotonicCounter(
      prev_mfaults,
      curr_mfaults,
      tid,
      QuickLogConstants::QL_THREAD_FAULTS_MAJOR);
}

inline void logCPUWaitTimeCounter(
  long prevCpuWaitTime,
  long currCpuWaitTime,
  int tid) {
  const auto kThresholdMs = 1;
  logMonotonicCounter(
    prevCpuWaitTime,
    currCpuWaitTime,
    tid,
    QuickLogConstants::THREAD_WAIT_IN_RUNQUEUE_TIME,
    kThresholdMs);
}

static int64_t loadDecimal(int64_t load) {
  constexpr int64_t kLoadShift = 1 << SI_LOAD_SHIFT;
  return (load / kLoadShift) * 1000 + (load % kLoadShift) * 1000 / kLoadShift;
}

void logSysinfo() {
  auto& logger = Logger::get();
  struct sysinfo info{};

  if (syscall(__NR_sysinfo, &info) < 0) {
    FBLOGE("Couldn't get sysinfo!");
    return;
  }

  logCounter(
    logger,
    QuickLogConstants::LOADAVG_1M,
    loadDecimal(info.loads[0])
  );
  logCounter(
    logger,
    QuickLogConstants::LOADAVG_5M,
    loadDecimal(info.loads[1])
  );
  logCounter(
    logger,
    QuickLogConstants::LOADAVG_15M,
    loadDecimal(info.loads[2])
  );
  logCounter(
    logger,
    QuickLogConstants::NUM_PROCS,
    info.procs
  );
  logCounter(
    logger,
    QuickLogConstants::TOTAL_MEM,
    (int64_t) info.totalram * (int64_t) info.mem_unit
  );
  logCounter(
    logger,
    QuickLogConstants::FREE_MEM,
    (int64_t) info.freeram * (int64_t) info.mem_unit
  );
  logCounter(
    logger,
    QuickLogConstants::SHARED_MEM,
    (int64_t) info.sharedram * (int64_t) info.mem_unit
  );
  logCounter(
    logger,
    QuickLogConstants::BUFFER_MEM,
    (int64_t) info.bufferram * (int64_t) info.mem_unit
  );
}

void logMallinfo() {
  auto& logger = Logger::get();
  struct mallinfo info = mallinfo();

  logCounter(
    logger,
    QuickLogConstants::ALLOC_MMAP_BYTES,
    info.hblkhd
  );
  logCounter(
    logger,
    QuickLogConstants::ALLOC_MAX_BYTES,
    info.usmblks
  );
  logCounter(
    logger,
    QuickLogConstants::ALLOC_TOTAL_BYTES,
    info.uordblks
  );
  logCounter(
    logger,
    QuickLogConstants::ALLOC_FREE_BYTES,
    info.fordblks
  );
}

void threadCountersCallback(
  uint32_t tid,
  util::ThreadStatInfo& prevInfo,
  util::ThreadStatInfo& currInfo) {

  if (prevInfo.highPrecisionCpuTimeMs != 0 &&
      (currInfo.availableStatsMask & util::StatType::HIGH_PRECISION_CPU_TIME)) {
    // Don't log the initial value
    logCPUTimeCounter(
        prevInfo.highPrecisionCpuTimeMs, currInfo.highPrecisionCpuTimeMs, tid);
  } else if (
      prevInfo.cpuTimeMs != 0 &&
      (currInfo.availableStatsMask & util::StatType::CPU_TIME)) {
    // Don't log the initial value
    logCPUTimeCounter(prevInfo.cpuTimeMs, currInfo.cpuTimeMs, tid);
  }
  if (prevInfo.waitToRunTimeMs != 0 &&
      (currInfo.availableStatsMask & util::StatType::WAIT_TO_RUN_TIME)) {
    logCPUWaitTimeCounter(
        prevInfo.waitToRunTimeMs, currInfo.waitToRunTimeMs, tid);
  }
  if (currInfo.availableStatsMask & util::StatType::MAJOR_FAULTS) {
    logThreadMajorFaults(prevInfo.majorFaults, currInfo.majorFaults, tid);
  }
  if (currInfo.availableStatsMask & util::StatType::NR_VOLUNTARY_SWITCHES) {
    logMonotonicCounter(
        prevInfo.nrVoluntarySwitches,
        currInfo.nrVoluntarySwitches,
        tid,
        QuickLogConstants::CONTEXT_SWITCHES_VOLUNTARY);
  }
  if (currInfo.availableStatsMask & util::StatType::NR_INVOLUNTARY_SWITCHES) {
    logMonotonicCounter(
        prevInfo.nrInvoluntarySwitches,
        currInfo.nrInvoluntarySwitches,
        tid,
        QuickLogConstants::CONTEXT_SWITCHES_INVOLUNTARY);
  }
  if (currInfo.availableStatsMask & util::StatType::IOWAIT_SUM) {
    logMonotonicCounter(
        prevInfo.iowaitSum,
        currInfo.iowaitSum,
        tid,
        QuickLogConstants::IOWAIT_TIME);
  }
  if (currInfo.availableStatsMask & util::StatType::IOWAIT_COUNT) {
    logMonotonicCounter(
        prevInfo.iowaitCount,
        currInfo.iowaitCount,
        tid,
        QuickLogConstants::IOWAIT_COUNT);
  }
}
} // anonymous

local_ref<SystemCounterThread::jhybriddata> SystemCounterThread::initHybrid(alias_ref<jclass>) {
  return makeCxxInstance();
}

void SystemCounterThread::registerNatives() {
  registerHybrid({
    makeNativeMethod("initHybrid", SystemCounterThread::initHybrid),
    makeNativeMethod(
      "logCounters",
      SystemCounterThread::logCounters),
    makeNativeMethod(
      "logThreadCounters",
      SystemCounterThread::logThreadCounters),
    makeNativeMethod(
      "logTraceAnnotations",
      SystemCounterThread::logTraceAnnotations),
  });
}

void SystemCounterThread::logCounters () {
  logProcessCounters();
  logThreadCounters(kTidAllThreads);
  logSysinfo();
  logMallinfo();
}

void SystemCounterThread::logThreadCounters(int tid) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (tid == kTidAllThreads) {
    cache_.forEach(&threadCountersCallback, kAllThreadsStatsMask);
  } else {
    cache_.forThread(tid, &threadCountersCallback, kHighFreqStatsMask);
  }
}

void SystemCounterThread::logTraceAnnotations(int tid) {
  std::lock_guard<std::mutex> lock(mtx_);
  Logger::get().writeTraceAnnotation(
      QuickLogConstants::AVAILABLE_COUNTERS,
      cache_.getStatsAvailablilty(tid));
}

// Log CPU time and major faults via /proc/self/stat
void SystemCounterThread::logProcessCounters() {
  if (!processStatFile_) {
    processStatFile_.reset(new util::TaskStatFile("/proc/self/stat"));
  }

  auto& logger = Logger::get();

  auto prevInfo = processStatFile_->getInfo();
  util::TaskStatInfo currInfo;

  try {
    currInfo = processStatFile_->refresh();
  } catch (const std::system_error& e) {
    return;
  } catch (const std::runtime_error& e) {
    return;
  }

  if (prevInfo.cpuTime != 0) {
    // Don't log the initial value
    const auto kThresholdMs = 1;

    if (currInfo.cpuTime > prevInfo.cpuTime + kThresholdMs) {
      logCounter(
        logger,
        QuickLogConstants::PROC_CPU_TIME,
        currInfo.cpuTime,
        threadID());
    }
  }

  if (prevInfo.majorFaults != currInfo.majorFaults) {
    logCounter(
      logger,
      QuickLogConstants::PROC_SW_FAULTS_MAJOR,
      currInfo.majorFaults,
      threadID());
  }
}

} // profilo
} // facebook
