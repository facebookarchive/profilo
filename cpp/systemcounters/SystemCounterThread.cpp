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
#include <fb/log.h>
#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <profilo/log.h>
#include <util/SysFs.h>
#include <util/common.h>

#include <sys/syscall.h>
#include <sys/sysinfo.h>

#include <malloc.h>

using facebook::jni::alias_ref;
using facebook::jni::local_ref;
using facebook::profilo::util::StatType;

namespace facebook {
namespace profilo {

namespace {

constexpr auto kTidAllThreads = 0;

const auto kAllThreadsStatsMask = StatType::CPU_TIME | StatType::STATE |
    StatType::MAJOR_FAULTS | StatType::MINOR_FAULTS | StatType::KERNEL_CPU_TIME;

const auto kHighFreqStatsMask = StatType::CPU_TIME | StatType::STATE |
    StatType::MAJOR_FAULTS | StatType::CPU_NUM |
    StatType::HIGH_PRECISION_CPU_TIME | StatType::WAIT_TO_RUN_TIME |
    StatType::NR_VOLUNTARY_SWITCHES | StatType::NR_INVOLUNTARY_SWITCHES |
    StatType::IOWAIT_SUM | StatType::IOWAIT_COUNT;

inline void logCounter(
    Logger& logger,
    int32_t counter_name,
    int64_t value,
    int32_t thread_id,
    int64_t time) {
  logger.write(StandardEntry{
      .id = 0,
      .type = entries::COUNTER,
      .timestamp = time,
      .tid = thread_id,
      .callid = counter_name,
      .matchid = 0,
      .extra = value,
  });
}

inline void logCpuCoreCounter(
    Logger& logger,
    int32_t counter_name,
    int64_t value,
    int32_t core,
    int32_t thread_id,
    int64_t time) {
  logger.write(StandardEntry{
      .id = 0,
      .type = entries::CPU_COUNTER,
      .timestamp = time,
      .tid = thread_id,
      .callid = counter_name,
      .matchid = core,
      .extra = value,
  });
}

inline void logNonMonotonicCounter(
    int64_t prev_value,
    int64_t value,
    int32_t thread_id,
    int64_t time,
    int32_t quicklog_id) {
  if (prev_value == value) {
    return;
  }
  logCounter(Logger::get(), quicklog_id, value, thread_id, time);
}

inline void logMonotonicCounter(
    long prev,
    long curr,
    int tid,
    int64_t time,
    uint32_t quicklog_id,
    int threshold = 0) {
  if (curr > prev + threshold) {
    logCounter(Logger::get(), quicklog_id, curr, tid, time);
  }
}

inline void
logCPUTimeCounter(long prevCpuTime, long currCpuTime, int tid, int64_t time) {
  const auto kThresholdMs = 1;
  logMonotonicCounter(
      prevCpuTime,
      currCpuTime,
      tid,
      time,
      QuickLogConstants::THREAD_CPU_TIME,
      kThresholdMs);
}

inline void logCPUWaitTimeCounter(
    long prevCpuWaitTime,
    long currCpuWaitTime,
    int tid,
    int64_t time) {
  const auto kThresholdMs = 1;
  logMonotonicCounter(
      prevCpuWaitTime,
      currCpuWaitTime,
      tid,
      time,
      QuickLogConstants::THREAD_WAIT_IN_RUNQUEUE_TIME,
      kThresholdMs);
}

static int64_t loadDecimal(int64_t load) {
  constexpr int64_t kLoadShift = 1 << SI_LOAD_SHIFT;
  return (load / kLoadShift) * 1000 + (load % kLoadShift) * 1000 / kLoadShift;
}

void logSysinfo() {
  auto& logger = Logger::get();
  struct sysinfo info {};

  if (syscall(__NR_sysinfo, &info) < 0) {
    FBLOGE("Couldn't get sysinfo!");
    return;
  }
  auto time = monotonicTime();
  auto tid = threadID();

  logCounter(
      logger,
      QuickLogConstants::LOADAVG_1M,
      loadDecimal(info.loads[0]),
      tid,
      time);
  logCounter(
      logger,
      QuickLogConstants::LOADAVG_5M,
      loadDecimal(info.loads[1]),
      tid,
      time);
  logCounter(
      logger,
      QuickLogConstants::LOADAVG_15M,
      loadDecimal(info.loads[2]),
      tid,
      time);
  logCounter(logger, QuickLogConstants::NUM_PROCS, info.procs, tid, time);
  logCounter(
      logger,
      QuickLogConstants::TOTAL_MEM,
      (int64_t)info.totalram * (int64_t)info.mem_unit,
      tid,
      time);
  logCounter(
      logger,
      QuickLogConstants::FREE_MEM,
      (int64_t)info.freeram * (int64_t)info.mem_unit,
      tid,
      time);
  logCounter(
      logger,
      QuickLogConstants::SHARED_MEM,
      (int64_t)info.sharedram * (int64_t)info.mem_unit,
      tid,
      time);
  logCounter(
      logger,
      QuickLogConstants::BUFFER_MEM,
      (int64_t)info.bufferram * (int64_t)info.mem_unit,
      tid,
      time);
}

void logMallinfo() {
  auto& logger = Logger::get();
  struct mallinfo info = mallinfo();
  auto time = monotonicTime();
  auto tid = threadID();

  logCounter(
      logger, QuickLogConstants::ALLOC_MMAP_BYTES, info.hblkhd, tid, time);
  logCounter(
      logger, QuickLogConstants::ALLOC_MAX_BYTES, info.usmblks, tid, time);
  logCounter(
      logger, QuickLogConstants::ALLOC_TOTAL_BYTES, info.uordblks, tid, time);
  logCounter(
      logger, QuickLogConstants::ALLOC_FREE_BYTES, info.fordblks, tid, time);
}

void threadCountersCallback(
    uint32_t tid,
    util::ThreadStatInfo& prevInfo,
    util::ThreadStatInfo& currInfo) {
  if (prevInfo.highPrecisionCpuTimeMs != 0 &&
      (currInfo.availableStatsMask & StatType::HIGH_PRECISION_CPU_TIME)) {
    // Don't log the initial value
    logCPUTimeCounter(
        prevInfo.highPrecisionCpuTimeMs,
        currInfo.highPrecisionCpuTimeMs,
        tid,
        currInfo.monotonicStatTime);
  } else if (
      prevInfo.cpuTimeMs != 0 &&
      (currInfo.availableStatsMask & StatType::CPU_TIME)) {
    // Don't log the initial value
    logCPUTimeCounter(
        prevInfo.cpuTimeMs,
        currInfo.cpuTimeMs,
        tid,
        currInfo.monotonicStatTime);
  }
  if (prevInfo.waitToRunTimeMs != 0 &&
      (currInfo.availableStatsMask & StatType::WAIT_TO_RUN_TIME)) {
    logCPUWaitTimeCounter(
        prevInfo.waitToRunTimeMs,
        currInfo.waitToRunTimeMs,
        tid,
        currInfo.monotonicStatTime);
  }
  if (currInfo.availableStatsMask & StatType::MAJOR_FAULTS) {
    logMonotonicCounter(
        prevInfo.majorFaults,
        currInfo.majorFaults,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::QL_THREAD_FAULTS_MAJOR);
  }
  if (currInfo.availableStatsMask & StatType::NR_VOLUNTARY_SWITCHES) {
    logMonotonicCounter(
        prevInfo.nrVoluntarySwitches,
        currInfo.nrVoluntarySwitches,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::CONTEXT_SWITCHES_VOLUNTARY);
  }
  if (currInfo.availableStatsMask & StatType::NR_INVOLUNTARY_SWITCHES) {
    logMonotonicCounter(
        prevInfo.nrInvoluntarySwitches,
        currInfo.nrInvoluntarySwitches,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::CONTEXT_SWITCHES_INVOLUNTARY);
  }
  if (currInfo.availableStatsMask & StatType::IOWAIT_SUM) {
    logMonotonicCounter(
        prevInfo.iowaitSum,
        currInfo.iowaitSum,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::IOWAIT_TIME);
  }
  if (currInfo.availableStatsMask & StatType::IOWAIT_COUNT) {
    logMonotonicCounter(
        prevInfo.iowaitCount,
        currInfo.iowaitCount,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::IOWAIT_COUNT);
  }
  if (currInfo.availableStatsMask & StatType::CPU_NUM) {
    logNonMonotonicCounter(
        prevInfo.cpuNum,
        currInfo.cpuNum,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::THREAD_CPU_NUM);
  }
  if (currInfo.availableStatsMask & StatType::KERNEL_CPU_TIME) {
    logMonotonicCounter(
        prevInfo.kernelCpuTimeMs,
        currInfo.kernelCpuTimeMs,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::THREAD_KERNEL_CPU_TIME);
  }
  if (currInfo.availableStatsMask & StatType::MINOR_FAULTS) {
    logMonotonicCounter(
        prevInfo.minorFaults,
        currInfo.minorFaults,
        tid,
        currInfo.monotonicStatTime,
        QuickLogConstants::THREAD_SW_FAULTS_MINOR);
  }
}
} // namespace

local_ref<SystemCounterThread::jhybriddata> SystemCounterThread::initHybrid(
    alias_ref<jclass>) {
  return makeCxxInstance();
}

void SystemCounterThread::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", SystemCounterThread::initHybrid),
      makeNativeMethod("logCounters", SystemCounterThread::logCounters),
      makeNativeMethod(
          "logThreadCounters", SystemCounterThread::logThreadCounters),
      makeNativeMethod(
          "logTraceAnnotations", SystemCounterThread::logTraceAnnotations),
  });
}

void SystemCounterThread::logCounters() {
  logProcessCounters();
  logThreadCounters(kTidAllThreads);
  logSysinfo();
  logMallinfo();
  logVmStatCounters();
}

void SystemCounterThread::logThreadCounters(int tid) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (tid == kTidAllThreads) {
    cache_.forEach(&threadCountersCallback, kAllThreadsStatsMask);
  } else {
    cache_.forThread(tid, &threadCountersCallback, kHighFreqStatsMask);
    logCpuFrequencyInfo();
  }
}

void SystemCounterThread::logCpuFrequencyInfo() {
  static bool freqStatsAvailable = true;
  if (!freqStatsAvailable) {
    return;
  }
  static auto cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
  static auto tid = threadID();
  if (cpu_cores <= 0) {
    freqStatsAvailable = false;
    return;
  }
  try {
    auto& logger = Logger::get();
    if (!cpuFrequencyStats_) {
      cpuFrequencyStats_.reset(new util::CpuFrequencyStats(cpu_cores));
      // Log max frequency only once
      for (int core = 0; core < cpu_cores; ++core) {
        int64_t maxFrequency = cpuFrequencyStats_->getMaxCpuFrequency(core);
        logCpuCoreCounter(
            logger,
            QuickLogConstants::MAX_CPU_CORE_FREQUENCY,
            maxFrequency,
            core,
            tid,
            monotonicTime());
      }
    }
    for (int core = 0; core < cpu_cores; ++core) {
      int64_t prev = cpuFrequencyStats_->getCachedCpuFrequency(core);
      int64_t curr = cpuFrequencyStats_->refresh(core);
      if (prev == curr) {
        continue;
      }

      logCpuCoreCounter(
          logger,
          QuickLogConstants::CPU_CORE_FREQUENCY,
          curr,
          core,
          tid,
          monotonicTime());
    }
    extraAvailableCounters_ |= StatType::CPU_FREQ;
  } catch (std::exception const& e) {
    freqStatsAvailable = false;
  }
}

void SystemCounterThread::logTraceAnnotations(int tid) {
  std::lock_guard<std::mutex> lock(mtx_);
  Logger::get().writeTraceAnnotation(
      QuickLogConstants::AVAILABLE_COUNTERS,
      cache_.getStatsAvailabililty(tid) | extraAvailableCounters_);
}

// Log CPU time and major faults via /proc/self/stat
void SystemCounterThread::logProcessCounters() {
  if (!processStatFile_) {
    processStatFile_.reset(new util::TaskStatFile("/proc/self/stat"));
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

  if (prevInfo.cpuTime != 0) {
    // Don't log the initial value
    const auto kThresholdMs = 1;

    logMonotonicCounter(
        prevInfo.cpuTime,
        currInfo.cpuTime,
        tid,
        time,
        QuickLogConstants::PROC_CPU_TIME,
        kThresholdMs);

    logMonotonicCounter(
        prevInfo.kernelCpuTimeMs,
        currInfo.kernelCpuTimeMs,
        tid,
        time,
        QuickLogConstants::PROC_KERNEL_CPU_TIME);
  }

  logMonotonicCounter(
      prevInfo.majorFaults,
      currInfo.majorFaults,
      tid,
      time,
      QuickLogConstants::PROC_SW_FAULTS_MAJOR);

  logMonotonicCounter(
      prevInfo.minorFaults,
      currInfo.minorFaults,
      tid,
      time,
      QuickLogConstants::PROC_SW_FAULTS_MINOR);
}

void SystemCounterThread::logVmStatCounters() {
  if (!vmStats_) {
    vmStats_.reset(new util::VmStatFile());
  }

  auto prevInfo = vmStats_->getInfo();
  util::VmStatInfo currInfo;
  try {
    currInfo = vmStats_->refresh();
  } catch (const std::system_error& e) {
    return;
  } catch (const std::runtime_error& e) {
    return;
  }

  auto time = monotonicTime();
  auto tid = threadID();

  logNonMonotonicCounter(
      prevInfo.nrFreePages,
      currInfo.nrFreePages,
      tid,
      time,
      QuickLogConstants::VMSTAT_NR_FREE_PAGES);
  logNonMonotonicCounter(
      prevInfo.nrDirty,
      currInfo.nrDirty,
      tid,
      time,
      QuickLogConstants::VMSTAT_NR_DIRTY);
  logNonMonotonicCounter(
      prevInfo.nrWriteback,
      currInfo.nrWriteback,
      tid,
      time,
      QuickLogConstants::VMSTAT_NR_WRITEBACK);
  logMonotonicCounter(
      prevInfo.pgPgIn,
      currInfo.pgPgIn,
      tid,
      time,
      QuickLogConstants::VMSTAT_PGPGIN);
  logMonotonicCounter(
      prevInfo.pgPgOut,
      currInfo.pgPgOut,
      tid,
      time,
      QuickLogConstants::VMSTAT_PGPGOUT);
  logMonotonicCounter(
      prevInfo.pgMajFault,
      currInfo.pgMajFault,
      tid,
      time,
      QuickLogConstants::VMSTAT_PGMAJFAULT);
  logMonotonicCounter(
      prevInfo.allocStall,
      currInfo.allocStall,
      tid,
      time,
      QuickLogConstants::VMSTAT_ALLOCSTALL);
  logMonotonicCounter(
      prevInfo.pageOutrun,
      currInfo.pageOutrun,
      tid,
      time,
      QuickLogConstants::VMSTAT_PAGEOUTRUN);
  logMonotonicCounter(
      prevInfo.kswapdSteal,
      currInfo.kswapdSteal,
      tid,
      time,
      QuickLogConstants::VMSTAT_KSWAPD_STEAL);
}

} // namespace profilo
} // namespace facebook
