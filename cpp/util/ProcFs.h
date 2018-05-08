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

#include <array>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <map>
#include <memory>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <util/BaseStatFile.h>

namespace facebook {
namespace profilo {
namespace util {

enum ThreadState: int {
  TS_RUNNING = 1,       // R
  TS_SLEEPING = 2,      // S
  TS_WAITING = 3,       // D
  TS_ZOMBIE = 4,        // Z
  TS_STOPPED = 5,       // T
  TS_TRACING_STOP = 6,  // t
  TS_PAGING = 7,        // W
  TS_DEAD = 8,          // X, x
  TS_WAKEKILL = 9,      // K
  TS_WAKING = 10,       // W
  TS_PARKED = 11,       // P

  TS_UNKNOWN = 0,
};

// Struct for data from /proc/self/task/<pid>/stat
struct TaskStatInfo {
  long cpuTime;
  ThreadState state;
  long majorFaults;
  long cpuNum;
  long kernelCpuTimeMs;
  long minorFaults;

  TaskStatInfo();
};

// Struct for data from /proc/self/task/<pid>/schedstat
struct SchedstatInfo {
  uint32_t cpuTimeMs;
  uint32_t waitToRunTimeMs;

  SchedstatInfo();
};

// Struct for data from /proc/self/task/<pid>/sched
struct SchedInfo {
  uint32_t nrVoluntarySwitches;
  uint32_t nrInvoluntarySwitches;
  uint32_t iowaitSum;
  uint32_t iowaitCount;

  SchedInfo();
};

// Consolidated stats from different stat files
struct ThreadStatInfo {
  // STAT
  long cpuTimeMs;
  ThreadState state;
  long majorFaults;
  long cpuNum;
  long kernelCpuTimeMs;
  long minorFaults;
  // SCHEDSTAT
  long highPrecisionCpuTimeMs;
  long waitToRunTimeMs;
  // SCHED
  long nrVoluntarySwitches;
  long nrInvoluntarySwitches;
  long iowaitSum;
  long iowaitCount;

  uint32_t availableStatsMask;

  ThreadStatInfo();
};

using stats_callback_fn = std::function<void (
  uint32_t, /* tid */
  ThreadStatInfo &, /* previous stats */
  ThreadStatInfo & /* current stats */)>;

using ThreadList = std::unordered_set<uint32_t>;
ThreadList threadListFromProcFs();

using FdList = std::unordered_set<uint32_t>;
FdList fdListFromProcFs();

std::string getThreadName(uint32_t thread_id);

TaskStatInfo getStatInfo(uint32_t tid);

class TaskStatFile: public BaseStatFile<TaskStatInfo> {
public:
  explicit TaskStatFile(uint32_t tid);
  explicit TaskStatFile(std::string path) : BaseStatFile(path) {}

  TaskStatInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

class TaskSchedstatFile: public BaseStatFile<SchedstatInfo> {
public:
  explicit TaskSchedstatFile(uint32_t tid);
  explicit TaskSchedstatFile(std::string path) : BaseStatFile(path) {}

  SchedstatInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

class TaskSchedFile: public BaseStatFile<SchedInfo> {
public:
  explicit TaskSchedFile(uint32_t tid);
  explicit TaskSchedFile(std::string path) : BaseStatFile(path),
    value_offsets_(),
    initialized_(false),
    value_size_(),
    availableStatsMask(0) {}

  SchedInfo doRead(int fd, uint32_t requested_stats_mask) override;

private:
  std::vector<std::pair<int32_t, int32_t>> value_offsets_;
  bool initialized_;
  int32_t value_size_;
public:
  int32_t availableStatsMask;
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
    uint32_t requested_stats_mask);

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

} // util
} // profilo
} // facebook
