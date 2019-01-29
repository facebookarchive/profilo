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

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <array>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <util/BaseStatFile.h>

namespace facebook {
namespace profilo {
namespace util {

enum ThreadState : int {
  TS_RUNNING = 1, // R
  TS_SLEEPING = 2, // S
  TS_WAITING = 3, // D
  TS_ZOMBIE = 4, // Z
  TS_STOPPED = 5, // T
  TS_TRACING_STOP = 6, // t
  TS_PAGING = 7, // W
  TS_DEAD = 8, // X, x
  TS_WAKEKILL = 9, // K
  TS_WAKING = 10, // W
  TS_PARKED = 11, // P

  TS_UNKNOWN = 0,
};

// Struct for data from /proc/self/task/<pid>/stat
struct TaskStatInfo {
  uint64_t cpuTime;
  ThreadState state;
  uint64_t majorFaults;
  uint8_t cpuNum;
  uint64_t kernelCpuTimeMs;
  uint64_t minorFaults;

  TaskStatInfo();
};

// Struct for data from /proc/self/task/<pid>/schedstat
struct SchedstatInfo {
  uint64_t cpuTimeMs;
  uint64_t waitToRunTimeMs;

  SchedstatInfo();
};

// Struct for data from /proc/self/task/<pid>/sched
struct SchedInfo {
  uint64_t nrVoluntarySwitches;
  uint64_t nrInvoluntarySwitches;
  uint64_t iowaitSum;
  uint64_t iowaitCount;

  SchedInfo();
};

// Struct for data from /proc/vmstat
struct VmStatInfo {
  uint64_t nrFreePages;
  uint64_t nrDirty;
  uint64_t nrWriteback;
  uint64_t pgPgIn;
  uint64_t pgPgOut;
  uint64_t pgMajFault;
  uint64_t allocStall;
  uint64_t pageOutrun;
  uint64_t kswapdSteal;

  VmStatInfo();
};

// Consolidated stats from different stat files
struct ThreadStatInfo {
  // monotonic clock value when this was captured
  uint64_t monotonicStatTime;

  // This bitmap contains information about which stats values were changed in
  // the previous sample. Every bit corresponds to a StatType identifier:
  //  1 - the value moved in the last sample
  //  0 - the value remained unchanged with respect to the preceding sample
  //  value.
  uint32_t statChangeMask;

  // STAT
  uint64_t cpuTimeMs;
  ThreadState state;
  uint64_t majorFaults;
  uint64_t cpuNum;
  uint64_t kernelCpuTimeMs;
  uint64_t minorFaults;
  // SCHEDSTAT
  uint64_t highPrecisionCpuTimeMs;
  uint64_t waitToRunTimeMs;
  // SCHED
  uint64_t nrVoluntarySwitches;
  uint64_t nrInvoluntarySwitches;
  uint64_t iowaitSum;
  uint64_t iowaitCount;

  uint32_t availableStatsMask;

  ThreadStatInfo();
};

using stats_callback_fn = std::function<void(
    uint32_t, /* tid */
    ThreadStatInfo&, /* previous stats */
    ThreadStatInfo& /* current stats */)>;

using ThreadList = std::unordered_set<uint32_t>;
ThreadList threadListFromProcFs();

using FdList = std::unordered_set<uint32_t>;
FdList fdListFromProcFs();

std::string getThreadName(uint32_t thread_id);

TaskStatInfo getStatInfo(uint32_t tid);

class TaskStatFile : public BaseStatFile<TaskStatInfo> {
 public:
  explicit TaskStatFile(uint32_t tid);
  explicit TaskStatFile(std::string path) : BaseStatFile(path) {}

  TaskStatInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

class TaskSchedstatFile : public BaseStatFile<SchedstatInfo> {
 public:
  explicit TaskSchedstatFile(uint32_t tid);
  explicit TaskSchedstatFile(std::string path) : BaseStatFile(path) {}

  SchedstatInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

class TaskSchedFile : public BaseStatFile<SchedInfo> {
 public:
  explicit TaskSchedFile(uint32_t tid);
  explicit TaskSchedFile(std::string path)
      : BaseStatFile(path),
        value_offsets_(),
        initialized_(false),
        value_size_(),
        buffer_(),
        availableStatsMask(0) {}

  SchedInfo doRead(int fd, uint32_t requested_stats_mask) override;

 private:
  static const size_t kMaxStatFileLength = 4096;

  std::vector<std::pair<int32_t, int32_t>> value_offsets_;
  bool initialized_;
  int32_t value_size_;
  char buffer_[kMaxStatFileLength];

 public:
  int32_t availableStatsMask;
};

class VmStatFile : public BaseStatFile<VmStatInfo> {
 public:
  explicit VmStatFile(std::string path);
  explicit VmStatFile() : VmStatFile("/proc/vmstat") {}

  VmStatInfo doRead(int fd, uint32_t requested_stats_mask) override;

 private:
  void recalculateOffsets();

  static const auto kNotFound = -1;
  static const auto kNotSet = -2;
  static const size_t kMaxStatFileLength = 4096;

  struct Key {
    const char* name;
    uint8_t length;
    int16_t offset;
    uint64_t& stat_field;
  };

  char buffer_[kMaxStatFileLength];
  size_t read_;
  VmStatInfo stat_info_;
  std::vector<Key> keys_;
};

// Consolidated stat files manager class
class ThreadStatHolder {
 public:
  explicit ThreadStatHolder(uint32_t tid);

  ThreadStatInfo refresh(uint32_t requested_stats_mask);
  ThreadStatInfo getInfo();

 private:
  std::unique_ptr<TaskStatFile> stat_file_;
  std::unique_ptr<TaskSchedstatFile> schedstat_file_;
  std::unique_ptr<TaskSchedFile> sched_file_;
  ThreadStatInfo last_info_;
  uint8_t availableStatFilesMask_;
  uint32_t availableStatsMask_;
  uint32_t tid_;
};

class ThreadCache {
 public:
  ThreadCache() = default;

  // Execute `function` for all currently existing threads.
  void forEach(
      stats_callback_fn callback,
      uint32_t requested_stats_mask,
      const std::unordered_set<int32_t>* black_list = nullptr);

  void forThread(
      uint32_t tid,
      stats_callback_fn callback,
      uint32_t requested_stats_mask);

  int32_t getStatsAvailabililty(int32_t tid);

  ThreadStatInfo getRecentStats(int32_t tid);

  void clear();

 private:
  std::unordered_map<uint32_t, ThreadStatHolder> cache_;
};

} // namespace util
} // namespace profilo
} // namespace facebook
