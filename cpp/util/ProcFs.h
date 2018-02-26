// Copyright 2004-present Facebook. All Rights Reserved.

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

namespace facebook {
namespace profilo {
namespace util {

// May not be needed afterwards
enum StatFileType: int8_t {
  STAT      = 1,
  SCHEDSTAT = 1 << 1,
  SCHED     = 1 << 2,
};

enum StatType: int32_t {
  CPU_TIME                = 1,
  STATE                   = 1 << 1,
  MAJOR_FAULTS            = 1 << 2,
  HIGH_PRECISION_CPU_TIME = 1 << 3,
  WAIT_TO_RUN_TIME        = 1 << 4,
  NR_VOLUNTARY_SWITCHES   = 1 << 5,
  NR_INVOLUNTARY_SWITCHES = 1 << 6,
  IOWAIT_SUM              = 1 << 7,
  IOWAIT_COUNT            = 1 << 8,
};

static const std::array<int32_t, 5> kFileStats = {
  0,
  /*STAT*/ StatType::CPU_TIME | StatType::STATE | StatType::MAJOR_FAULTS,
  /*SCHEDSTAT*/ StatType::HIGH_PRECISION_CPU_TIME | StatType::WAIT_TO_RUN_TIME,
  0,
  /*SCHED*/ StatType::NR_VOLUNTARY_SWITCHES |
    StatType::NR_INVOLUNTARY_SWITCHES |
    StatType::IOWAIT_SUM |
    StatType::IOWAIT_COUNT
};

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

  TaskStatInfo();
};

// Struct for data from /proc/self/task/<pid>/schedstat
struct SchedstatInfo {
  uint32_t cpuTimeMs;
  uint32_t waitToRunTimeMs;

  SchedstatInfo();
};

// Struct for data from /proc/self/task/<pid>/schedstat
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

template<class StatInfo>
class BaseStatFile {
public:
  explicit BaseStatFile(std::string path):
    path_(path), fd_(-1), last_info_() {}

  BaseStatFile() = delete;
  BaseStatFile(const BaseStatFile&) = delete;
  BaseStatFile& operator=(const BaseStatFile&) = delete;

  BaseStatFile(BaseStatFile&&) = default;
  BaseStatFile& operator=(BaseStatFile&&) = default;
  virtual ~BaseStatFile() {
    if (fd_ != -1) {
      close(fd_); // silently swallow errors
      fd_ = -1;
    }
  }

  // Returns the last read StatInfo or a default-constructed
  // one, if never read.
  StatInfo getInfo() const {
    return last_info_;
  }

  // Re-reads the stat file, opening it if necessary.
  // Can throw std::system_error or std::runtime_error.
  //
  // Returns the new StatInfo.
  StatInfo refresh() {
    if (fd_ == -1) {
      fd_ = doOpen(path_);
    }

    if (lseek(fd_, 0, SEEK_SET)) {
      throw std::system_error(
        errno,
        std::system_category(),
        "Could not rewind file");
    }

    last_info_ = doRead(fd_);
    return last_info_;
  }

  int doOpen(const std::string& path) {
    int statFile = open(path.c_str(), O_SYNC|O_RDONLY);

    if (statFile == -1) {
      throw std::system_error(
        errno,
        std::system_category(),
        "Could not open stat file");
    }
    return statFile;
  }

protected:
  virtual StatInfo doRead(int fd) = 0;

private:
  std::string path_;
  int fd_;
  StatInfo last_info_;
};

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

  TaskStatInfo doRead(int fd) override;
};

class TaskSchedstatFile: public BaseStatFile<SchedstatInfo> {
public:
  explicit TaskSchedstatFile(uint32_t tid);

  SchedstatInfo doRead(int fd) override;
};

class TaskSchedFile: public BaseStatFile<SchedInfo> {
public:
  explicit TaskSchedFile(uint32_t tid);

  SchedInfo doRead(int fd) override;

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

  int32_t getStatsAvailablilty(int32_t tid);

  void clear();

private:
  std::unordered_map<uint32_t, ThreadStatHolder> cache_;
};

} // util
} // profilo
} // facebook
