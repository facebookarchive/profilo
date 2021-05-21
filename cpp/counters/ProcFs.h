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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <array>
#include <climits>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <counters/BaseStatFile.h>
#include <counters/Counter.h>
#include <profilo/MultiBufferLogger.h>
#include <profilo/util/ProcFsUtils.h>
#include <profilo/util/common.h>

using facebook::profilo::logger::MultiBufferLogger;

namespace facebook {
namespace profilo {
namespace counters {

using namespace util;

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

// data from /proc/self/task/<pid>/stat
struct TaskStatInfo {
  uint64_t cpuTime;
  ThreadState state;
  uint64_t majorFaults;
  uint8_t cpuNum;
  uint64_t kernelCpuTimeMs;
  uint64_t minorFaults;
  int16_t threadPriority;

  TaskStatInfo();
};

// data from /proc/self/task/<pid>/schedstat
struct SchedstatInfo {
  uint64_t cpuTimeMs;
  uint64_t waitToRunTimeMs;

  SchedstatInfo();
};

// data from /proc/self/task/<pid>/sched
struct SchedInfo {
  uint64_t nrVoluntarySwitches;
  uint64_t nrInvoluntarySwitches;
  uint64_t iowaitSum;
  uint64_t iowaitCount;

  SchedInfo();
};

// data from /proc/vmstat
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

// data from /proc/../statm
struct StatmInfo {
  uint64_t resident;
  uint64_t shared;
};

// data from /proc/meminfo
struct MeminfoInfo {
  uint64_t freeKB;
  uint64_t dirtyKB;
  uint64_t writebackKB;
  uint64_t cachedKB;
  uint64_t activeKB;
  uint64_t inactiveKB;
};

// Consolidated stats from different stat files
struct ThreadStatInfo {
  // STAT
  TraceCounter cpuTimeMs;
  TraceCounter state;
  TraceCounter majorFaults;
  TraceCounter cpuNum;
  TraceCounter kernelCpuTimeMs;
  TraceCounter minorFaults;
  TraceCounter threadPriority;
  // SCHEDSTAT
  TraceCounter highPrecisionCpuTimeMs;
  TraceCounter waitToRunTimeMs;
  // SCHED
  TraceCounter nrVoluntarySwitches;
  TraceCounter nrInvoluntarySwitches;
  TraceCounter iowaitSum;
  TraceCounter iowaitCount;

  uint32_t availableStatsMask;

  static ThreadStatInfo createThreadStatInfo(
      MultiBufferLogger& logger,
      int32_t tid);
};

TaskStatInfo getStatInfo(int32_t tid);

class TaskStatFile : public BaseStatFile<TaskStatInfo> {
 public:
  explicit TaskStatFile(int32_t tid);
  explicit TaskStatFile(std::string path) : BaseStatFile(path) {}

  TaskStatInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

class TaskSchedstatFile : public BaseStatFile<SchedstatInfo> {
 public:
  explicit TaskSchedstatFile(int32_t tid);
  explicit TaskSchedstatFile(std::string path) : BaseStatFile(path) {}

  SchedstatInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

class TaskSchedFile : public BaseStatFile<SchedInfo> {
 public:
  explicit TaskSchedFile(int32_t tid);
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

/**
 * Represents a file with one row per value where the values are structured as
 * "<key><variable amount of whitespace><value>\n"
 * and, most importantly, the keys are usually at the same offsets in the
 * file (thus files with left-padded values are best).
 *
 * This class will avoid doing linear scans all the time by only
 * calculating the offsets for each requested Key once. It will correctly
 * recalculate them if any of them change but if this happens too often
 * the caching is actually detrimental.
 */
template <class StatInfo>
class OrderedKeyedStatFile : public BaseStatFile<StatInfo> {
 public:
  struct Key {
    const char* name;
    uint8_t length;
    int16_t offset;
    // Pointer-to-member: which field of StatInfo should contain the key value
    uint64_t StatInfo::*stat_field;
  };

  explicit OrderedKeyedStatFile(std::string path, std::vector<Key> keys)
      : BaseStatFile<StatInfo>(path), keys_(keys) {}

  static const auto kNotFound = -1;
  static const auto kNotSet = -2;
  static const size_t kMaxStatFileLength = 4096;

 private:
  void recalculateOffsets() {
    bool found = false;
    char* end = nullptr;
    char* start = buffer_;

    auto nextKey = keys_.begin();
    auto keys_end = keys_.end();

    //
    // These nested loops implement the following logic with some extra
    // edge cases (kNotFound, etc)
    //
    // keys = [k1, k2, k3]
    // idx = 0
    //
    // for line in file.readlines():
    //   for search_idx in range(idx, len(keys)):
    //     if line.startswith(keys[search_idx]):
    //       <store key byte offset>
    //       idx = search_idx + 1
    //
    // That is, we always look for all the keys from the current one to the end
    // of the list. If we find one of the later keys, we skip straight to it
    // and later mark the keys we skipped over as kNotFound.
    //
    //
    while (nextKey < keys_end && (end = std::strchr(start, '\n'))) {
      if (std::distance(buffer_, end) > read_) {
        break;
      }

      // Skip not found keys
      while (nextKey->offset == kNotFound && nextKey < keys_end) {
        ++nextKey;
      }

      auto searchKey = nextKey;
      while (searchKey < keys_end) {
        if (std::strncmp(searchKey->name, start, searchKey->length) == 0) {
          searchKey->offset = std::distance(buffer_, start);
          found = true;
          nextKey = ++searchKey;
          break;
        }
        ++searchKey;
      }

      // Search for nextKey on the next line.
      start = end + 1;
    }

    if (!found) {
      throw std::runtime_error("No target fields found");
    }

    // Set not matched keys to kNotFound
    for (auto& key : keys_) {
      if (key.offset == kNotSet) {
        key.offset = kNotFound;
      }
    }
  }

  StatInfo doRead(int fd, uint32_t ignored) override {
    read_ = read(fd, buffer_, sizeof(buffer_) - 1);
    if (read_ < 0) {
      throw std::system_error(
          errno, std::system_category(), "Could not read stat file");
    }

    for (auto& key : keys_) {
      stat_info_.*(key.stat_field) = 0;
    }

    for (auto& key : keys_) {
      if (key.offset == kNotFound) {
        continue;
      }
      if (key.offset == kNotSet || key.offset >= read_ ||
          std::strncmp(key.name, buffer_ + key.offset, key.length) != 0) {
        recalculateOffsets();
      }
      errno = 0;
      char* endptr = nullptr;
      char* start = buffer_ + key.offset + key.length;
      auto value = parse_ull(start, &endptr);
      stat_info_.*(key.stat_field) += value;
    }

    return stat_info_;
  }

  char buffer_[kMaxStatFileLength]{};
  size_t read_{};
  StatInfo stat_info_{};
  std::vector<Key> keys_;
};

class ProcStatmFile : public BaseStatFile<StatmInfo> {
 public:
  explicit ProcStatmFile() : BaseStatFile("/proc/self/statm") {}
  explicit ProcStatmFile(std::string path) : BaseStatFile(path) {}

  StatmInfo doRead(int fd, uint32_t requested_stats_mask) override;
};

struct VmStatFile : public OrderedKeyedStatFile<VmStatInfo> {
  explicit VmStatFile(std::string path);
  VmStatFile();
};

struct MeminfoFile : public OrderedKeyedStatFile<MeminfoInfo> {
  explicit MeminfoFile(std::string path);
  MeminfoFile();
};

// Consolidated stat files manager class
class ThreadStatHolder {
 public:
  explicit ThreadStatHolder(MultiBufferLogger& logger, int32_t tid);

  void sampleAndLog(uint32_t requested_stats_mask, int32_t tid);
  ThreadStatInfo& getInfo();

 private:
  std::unique_ptr<TaskStatFile> stat_file_;
  std::unique_ptr<TaskSchedstatFile> schedstat_file_;
  std::unique_ptr<TaskSchedFile> sched_file_;
  ThreadStatInfo last_info_;
  uint8_t availableStatFilesMask_;
  uint32_t availableStatsMask_;
  int32_t tid_;
};

class ThreadCache {
 public:
  ThreadCache(MultiBufferLogger& logger);

  // Execute `function` for all currently existing threads.
  void sampleAndLogForEach(
      uint32_t requested_stats_mask,
      const std::unordered_set<int32_t>* black_list = nullptr);

  void sampleAndLogForThread(int32_t tid, uint32_t requested_stats_mask);

  int32_t getStatsAvailabililty(int32_t tid);

  void clear();

 private:
  MultiBufferLogger& logger_;
  std::unordered_map<uint32_t, ThreadStatHolder> cache_;
};

} // namespace counters
} // namespace profilo
} // namespace facebook
