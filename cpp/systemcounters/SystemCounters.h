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

#include "common.h"

#include <counters/SysFs.h>
#include <fb/log.h>
#include <malloc.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <util/common.h>

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

template <typename Logger>
inline void logCpuCoreCounter(
    Logger& logger,
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

template <typename Logger>
class SystemCounters {
 private:
  void logSysinfo() {
    auto& logger = Logger::get();
    struct sysinfo info {};

    if (syscall(__NR_sysinfo, &info) < 0) {
      FBLOGE("Couldn't get sysinfo!");
      return;
    }
    auto time = monotonicTime();
    auto tid = threadID();

    logCounter<Logger>(
        logger,
        QuickLogConstants::LOADAVG_1M,
        loadDecimal(info.loads[0]),
        tid,
        time);
    logCounter<Logger>(
        logger,
        QuickLogConstants::LOADAVG_5M,
        loadDecimal(info.loads[1]),
        tid,
        time);
    logCounter<Logger>(
        logger,
        QuickLogConstants::LOADAVG_15M,
        loadDecimal(info.loads[2]),
        tid,
        time);
    logCounter<Logger>(
        logger, QuickLogConstants::NUM_PROCS, info.procs, tid, time);
    logCounter<Logger>(
        logger,
        QuickLogConstants::FREE_MEM,
        (int64_t)info.freeram * (int64_t)info.mem_unit,
        tid,
        time);
    logCounter<Logger>(
        logger,
        QuickLogConstants::SHARED_MEM,
        (int64_t)info.sharedram * (int64_t)info.mem_unit,
        tid,
        time);
    logCounter<Logger>(
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

  void logCpuFrequencyInfo() {
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
        cpuFrequencyStats_.reset(new CpuFrequencyStats(cpu_cores));
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

        logCpuCoreCounter<Logger>(
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

  void logVmStatCounters() {
    if (vmStatsTracingDisabled_) {
      return;
    }
    if (!vmStats_) {
      vmStats_.reset(new VmStatFile());
    }

    auto prevInfo = vmStats_->getInfo();
    VmStatInfo currInfo;
    try {
      currInfo = vmStats_->refresh();
      extraAvailableCounters_ |= kVmStatCountersMask;
    } catch (...) {
      vmStatsTracingDisabled_ = true;
      vmStats_.reset(nullptr);
      return;
    }

    auto time = monotonicTime();
    auto tid = threadID();
    Logger& logger = Logger::get();

    logMonotonicCounter<Logger>(
        prevInfo.pgPgIn,
        currInfo.pgPgIn,
        tid,
        time,
        QuickLogConstants::VMSTAT_PGPGIN,
        logger);
    logMonotonicCounter<Logger>(
        prevInfo.pgPgOut,
        currInfo.pgPgOut,
        tid,
        time,
        QuickLogConstants::VMSTAT_PGPGOUT,
        logger);
    logMonotonicCounter<Logger>(
        prevInfo.pgMajFault,
        currInfo.pgMajFault,
        tid,
        time,
        QuickLogConstants::VMSTAT_PGMAJFAULT,
        logger);
    logMonotonicCounter<Logger>(
        prevInfo.allocStall,
        currInfo.allocStall,
        tid,
        time,
        QuickLogConstants::VMSTAT_ALLOCSTALL,
        logger);
    logMonotonicCounter<Logger>(
        prevInfo.pageOutrun,
        currInfo.pageOutrun,
        tid,
        time,
        QuickLogConstants::VMSTAT_PAGEOUTRUN,
        logger);
    logMonotonicCounter<Logger>(
        prevInfo.kswapdSteal,
        currInfo.kswapdSteal,
        tid,
        time,
        QuickLogConstants::VMSTAT_KSWAPD_STEAL,
        logger);
  }

  void logMeminfoCounters() {
    if (meminfoTracingDisabled_) {
      return;
    }
    if (!meminfo_) {
      meminfo_.reset(new MeminfoFile());
    }

    auto prevInfo = meminfo_->getInfo();
    MeminfoInfo currInfo;
    try {
      currInfo = meminfo_->refresh();
      extraAvailableCounters_ |= kMeminfoCountersMask;
    } catch (...) {
      meminfoTracingDisabled_ = true;
      meminfo_.reset(nullptr);
      return;
    }

    auto time = monotonicTime();
    auto tid = threadID();
    Logger& logger = Logger::get();
    static constexpr auto kBytesInKB = 1024;

    logNonMonotonicCounter<Logger>(
        prevInfo.freeKB * kBytesInKB,
        currInfo.freeKB * kBytesInKB,
        tid,
        time,
        QuickLogConstants::MEMINFO_FREE,
        logger);
    logNonMonotonicCounter<Logger>(
        prevInfo.dirtyKB * kBytesInKB,
        currInfo.dirtyKB * kBytesInKB,
        tid,
        time,
        QuickLogConstants::MEMINFO_DIRTY,
        logger);
    logNonMonotonicCounter<Logger>(
        prevInfo.writebackKB * kBytesInKB,
        currInfo.writebackKB * kBytesInKB,
        tid,
        time,
        QuickLogConstants::MEMINFO_WRITEBACK,
        logger);
    logNonMonotonicCounter<Logger>(
        prevInfo.cachedKB * kBytesInKB,
        currInfo.cachedKB * kBytesInKB,
        tid,
        time,
        QuickLogConstants::MEMINFO_CACHED,
        logger);
    logNonMonotonicCounter<Logger>(
        prevInfo.activeKB * kBytesInKB,
        currInfo.activeKB * kBytesInKB,
        tid,
        time,
        QuickLogConstants::MEMINFO_ACTIVE,
        logger);
    logNonMonotonicCounter<Logger>(
        prevInfo.inactiveKB * kBytesInKB,
        currInfo.inactiveKB * kBytesInKB,
        tid,
        time,
        QuickLogConstants::MEMINFO_INACTIVE,
        logger);
  }

  std::unique_ptr<CpuFrequencyStats> cpuFrequencyStats_;
  std::unique_ptr<VmStatFile> vmStats_;
  std::unique_ptr<MeminfoFile> meminfo_;
  bool vmStatsTracingDisabled_;
  bool meminfoTracingDisabled_;
  int32_t extraAvailableCounters_;

 public:
  void logCounters() {
    logMallinfo();
    logSysinfo();
    logVmStatCounters();
    logMeminfoCounters();
  }

  void logHighFreqCounters() {
    logCpuFrequencyInfo();
  }

  int32_t getAvailableCounters() {
    return extraAvailableCounters_;
  }
};

} // namespace counters
} // namespace profilo
} // namespace facebook
