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

#include "SystemCounters.h"

#include <fb/log.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace facebook {
namespace profilo {
namespace counters {

namespace {

const auto kVmStatCountersMask = StatType::VMSTAT_NR_FREE_PAGES |
    StatType::VMSTAT_NR_DIRTY | StatType::VMSTAT_NR_WRITEBACK |
    StatType::VMSTAT_PGPGIN | StatType::VMSTAT_PGPGOUT |
    StatType::VMSTAT_PGMAJFAULT | StatType::VMSTAT_ALLOCSTALL |
    StatType::VMSTAT_PAGEOUTRUN | StatType::VMSTAT_KSWAPD_STEAL;

const auto kMeminfoCountersMask = StatType::MEMINFO_ACTIVE |
    StatType::MEMINFO_INACTIVE | StatType::MEMINFO_CACHED |
    StatType::MEMINFO_DIRTY | StatType::MEMINFO_WRITEBACK |
    StatType::MEMINFO_FREE;

static inline int64_t loadDecimal(int64_t load) {
  constexpr int64_t kLoadShift = 1 << SI_LOAD_SHIFT;
  return (load / kLoadShift) * 1000 + (load % kLoadShift) * 1000 / kLoadShift;
}

inline void logCpuCoreCounter(
    MultiBufferLogger& logger,
    int32_t counter_name,
    int64_t value,
    int32_t core,
    int32_t thread_id,
    int64_t time) {
  logger.write(StandardEntry{
      .id = 0,
      .type = EntryType::CPU_COUNTER,
      .timestamp = time,
      .tid = thread_id,
      .callid = counter_name,
      .matchid = core,
      .extra = value,
  });
}

} // namespace

void SystemCounters::logSysinfo(int64_t time) {
  struct sysinfo info {};
  if (syscall(__NR_sysinfo, &info) < 0) {
    FBLOGE("Couldn't get sysinfo!");
    return;
  }
  stats_.loadAvg1m.record(loadDecimal(info.loads[0]), time);
  stats_.loadAvg5m.record(loadDecimal(info.loads[1]), time);
  stats_.loadAvg15m.record(loadDecimal(info.loads[2]), time);
  stats_.numProcs.record(info.procs, time);
  stats_.freeMem.record((int64_t)info.freeram * (int64_t)info.mem_unit, time);
  stats_.sharedMem.record(
      (int64_t)info.sharedram * (int64_t)info.mem_unit, time);
  stats_.bufferMem.record(
      (int64_t)info.bufferram * (int64_t)info.mem_unit, time);
}

void SystemCounters::logMallinfo(int64_t time) {
  struct mallinfo info = mallinfo();
  stats_.allocMmapBytes.record(info.hblkhd, time);
  stats_.allocMaxBytes.record(info.usmblks, time);
  stats_.allocTotalBytes.record(info.uordblks, time);
  stats_.allocFreeBytes.record(info.fordblks, time);
}

void SystemCounters::logCpuFrequencyInfo(int64_t time, int32_t tid) {
  static bool freqStatsAvailable = true;
  if (!freqStatsAvailable) {
    return;
  }
  static auto cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (cpu_cores <= 0) {
    freqStatsAvailable = false;
    return;
  }
  try {
    if (!cpuFrequencyStats_) {
      cpuFrequencyStats_.reset(new CpuFrequencyStats(cpu_cores));
      // Log max frequency only once
      for (int core = 0; core < cpu_cores; ++core) {
        int64_t maxFrequency = cpuFrequencyStats_->getMaxCpuFrequency(core);
        logCpuCoreCounter(
            logger_,
            QuickLogConstants::MAX_CPU_CORE_FREQUENCY,
            maxFrequency,
            core,
            tid,
            time);
      }
    }
    for (int core = 0; core < cpu_cores; ++core) {
      int64_t prev = cpuFrequencyStats_->getCachedCpuFrequency(core);
      int64_t curr = cpuFrequencyStats_->refresh(core);
      if (prev == curr) {
        continue;
      }

      logCpuCoreCounter(
          logger_,
          QuickLogConstants::CPU_CORE_FREQUENCY,
          curr,
          core,
          tid,
          time);
    }
    extraAvailableCounters_ |= StatType::CPU_FREQ;
  } catch (std::exception const& e) {
    freqStatsAvailable = false;
  }
}

void SystemCounters::logVmStatCounters(int64_t time) {
  if (vmStatsTracingDisabled_) {
    return;
  }
  if (!vmStats_) {
    vmStats_.reset(new VmStatFile());
  }

  VmStatInfo currInfo;
  try {
    currInfo = vmStats_->refresh();
    extraAvailableCounters_ |= kVmStatCountersMask;
  } catch (...) {
    vmStatsTracingDisabled_ = true;
    vmStats_.reset(nullptr);
    return;
  }

  stats_.pgPgIn.record(currInfo.pgPgIn, time);
  stats_.pgPgOut.record(currInfo.pgPgOut, time);
  stats_.pgMajFault.record(currInfo.pgMajFault, time);
  stats_.allocStall.record(currInfo.allocStall, time);
  stats_.pageOutrun.record(currInfo.pageOutrun, time);
  stats_.kswapdSteal.record(currInfo.kswapdSteal, time);
}

void SystemCounters::logMeminfoCounters(int64_t time) {
  if (meminfoTracingDisabled_) {
    return;
  }
  if (!meminfo_) {
    meminfo_.reset(new MeminfoFile());
  }

  MeminfoInfo currInfo;
  try {
    currInfo = meminfo_->refresh();
    extraAvailableCounters_ |= kMeminfoCountersMask;
  } catch (...) {
    meminfoTracingDisabled_ = true;
    meminfo_.reset(nullptr);
    return;
  }

  static constexpr auto kBytesInKB = 1024;

  stats_.freeBytes.record(currInfo.freeKB * kBytesInKB, time);
  stats_.dirtyBytes.record(currInfo.dirtyKB * kBytesInKB, time);
  stats_.writebackBytes.record(currInfo.writebackKB * kBytesInKB, time);
  stats_.cachedBytes.record(currInfo.cachedKB * kBytesInKB, time);
  stats_.activeBytes.record(currInfo.activeKB * kBytesInKB, time);
  stats_.inactiveBytes.record(currInfo.inactiveKB * kBytesInKB, time);
}

void SystemCounters::logCounters() {
  auto time = monotonicTime();
  logMallinfo(time);
  logSysinfo(time);
  logVmStatCounters(time);
  logMeminfoCounters(time);
}

void SystemCounters::logHighFreqCounters() {
  logCpuFrequencyInfo(monotonicTime(), threadID());
}

} // namespace counters
} // namespace profilo
} // namespace facebook
