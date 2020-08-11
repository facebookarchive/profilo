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

#include <counters/Counter.h>
#include <counters/ProcFs.h>
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

struct SystemStats {
  // Mallinfo
  TraceCounter allocMmapBytes;
  TraceCounter allocMaxBytes;
  TraceCounter allocTotalBytes;
  TraceCounter allocFreeBytes;
  // Sysinfo
  TraceCounter loadAvg1m;
  TraceCounter loadAvg5m;
  TraceCounter loadAvg15m;
  TraceCounter numProcs;
  TraceCounter freeMem;
  TraceCounter sharedMem;
  TraceCounter bufferMem;
  // VmStat
  TraceCounter pgPgIn;
  TraceCounter pgPgOut;
  TraceCounter pgMajFault;
  TraceCounter allocStall;
  TraceCounter pageOutrun;
  TraceCounter kswapdSteal;
  // Meminfo
  TraceCounter freeBytes;
  TraceCounter dirtyBytes;
  TraceCounter writebackBytes;
  TraceCounter cachedBytes;
  TraceCounter activeBytes;
  TraceCounter inactiveBytes;
};

class SystemCounters {
 private:
  void logSysinfo(int64_t time);

  void logMallinfo(int64_t time);

  void logCpuFrequencyInfo(int64_t time, int32_t tid);

  void logVmStatCounters(int64_t time);

  void logMeminfoCounters(int64_t time);

  std::unique_ptr<CpuFrequencyStats> cpuFrequencyStats_;
  std::unique_ptr<VmStatFile> vmStats_;
  std::unique_ptr<MeminfoFile> meminfo_;
  bool vmStatsTracingDisabled_;
  bool meminfoTracingDisabled_;
  int32_t extraAvailableCounters_;
  SystemStats stats_;

 public:
  SystemCounters(int32_t pid = getpid())
      : cpuFrequencyStats_(),
        vmStats_(),
        meminfo_(),
        vmStatsTracingDisabled_(false),
        meminfoTracingDisabled_(false),
        extraAvailableCounters_(0),
        stats_(SystemStats{
            .allocMmapBytes =
                TraceCounter(QuickLogConstants::ALLOC_MMAP_BYTES, pid),
            .allocMaxBytes =
                TraceCounter(QuickLogConstants::ALLOC_MAX_BYTES, pid),
            .allocTotalBytes =
                TraceCounter(QuickLogConstants::ALLOC_TOTAL_BYTES, pid),
            .allocFreeBytes =
                TraceCounter(QuickLogConstants::ALLOC_FREE_BYTES, pid),
            .loadAvg1m = TraceCounter(QuickLogConstants::LOADAVG_1M, pid),
            .loadAvg5m = TraceCounter(QuickLogConstants::LOADAVG_5M, pid),
            .loadAvg15m = TraceCounter(QuickLogConstants::LOADAVG_15M, pid),
            .numProcs = TraceCounter(QuickLogConstants::NUM_PROCS, pid),
            .freeMem = TraceCounter(QuickLogConstants::FREE_MEM, pid),
            .sharedMem = TraceCounter(QuickLogConstants::SHARED_MEM, pid),
            .bufferMem = TraceCounter(QuickLogConstants::BUFFER_MEM, pid),
            .pgPgIn = TraceCounter(QuickLogConstants::VMSTAT_PGPGIN, pid),
            .pgPgOut = TraceCounter(QuickLogConstants::VMSTAT_PGPGOUT, pid),
            .pgMajFault =
                TraceCounter(QuickLogConstants::VMSTAT_PGMAJFAULT, pid),
            .allocStall =
                TraceCounter(QuickLogConstants::VMSTAT_ALLOCSTALL, pid),
            .pageOutrun =
                TraceCounter(QuickLogConstants::VMSTAT_PAGEOUTRUN, pid),
            .kswapdSteal =
                TraceCounter(QuickLogConstants::VMSTAT_KSWAPD_STEAL, pid),
            .freeBytes = TraceCounter(QuickLogConstants::MEMINFO_FREE, pid),
            .dirtyBytes = TraceCounter(QuickLogConstants::MEMINFO_DIRTY, pid),
            .writebackBytes =
                TraceCounter(QuickLogConstants::MEMINFO_WRITEBACK, pid),
            .cachedBytes = TraceCounter(QuickLogConstants::MEMINFO_CACHED, pid),
            .activeBytes = TraceCounter(QuickLogConstants::MEMINFO_ACTIVE, pid),
            .inactiveBytes =
                TraceCounter(QuickLogConstants::MEMINFO_INACTIVE, pid)}) {}

  void logCounters();

  void logHighFreqCounters();

  int32_t getAvailableCounters() {
    return extraAvailableCounters_;
  }
};

} // namespace counters
} // namespace profilo
} // namespace facebook
