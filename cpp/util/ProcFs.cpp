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

#include <util/ProcFs.h>

#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>
#include <array>
#include <algorithm>
#include <util/common.h>

namespace facebook {
namespace profilo {
namespace util {

static constexpr int kMaxProcFileLength = 64;

// Return all the numeric items in the folder passed as parameter.
// Non-numeric items are ignored.
static std::unordered_set<uint32_t> numericFolderItems(
  const char* folder
) {
  DIR* dir = opendir(folder);
  if (dir == nullptr) {
    throw std::system_error(errno, std::system_category());
  }
  std::unordered_set<uint32_t> items;

  // We don't want to use readdir(3) since that may use global state in a non-thread-safe manner.
  // Instead, we need to use readdir_r(3) which only uses user-supplied buffers.

  // There's a fairly convoluted POSIX-compliant way via fpathconf(3) to allocate a dirent
  // with a big enough buffer for the filename.
  // Or, we can exploit the fact that this is Linux and that dirent.d_name is char[255].

  dirent child {};
  dirent* result = nullptr;

  errno = 0;
  while (readdir_r(dir, &child, &result) == 0 && result != nullptr) {

    // Skip navigation entries
    if (strcmp(".", child.d_name) == 0 ||
        strcmp("..", child.d_name) == 0) {
      continue;
    }

    errno = 0;
    char* endptr = nullptr;
    uint32_t item = strtoul(child.d_name, &endptr, /*base*/ 10);
    if (errno != 0 || *endptr != '\0') {
      continue; // unable to parse item
    }
    items.emplace(item);
  }
  if (errno != 0 || closedir(dir) != 0) {
    throw std::system_error(errno, std::system_category(), "readdir or closedir");
  }
  return items;
}

ThreadList threadListFromProcFs() {
  return ThreadList(numericFolderItems("/proc/self/task/"));
}

FdList fdListFromProcFs() {
  return FdList(numericFolderItems("/proc/self/fd/"));
}

std::string getThreadName(uint32_t thread_id) {
  char threadNamePath[kMaxProcFileLength]{};
  int bytesWritten =
    snprintf(&threadNamePath[0], kMaxProcFileLength, "/proc/self/task/%d/comm", thread_id);
  if (bytesWritten < 0 || bytesWritten >= kMaxProcFileLength) {
    errno = 0;
    return "";
  }
  FILE* threadNameFile = fopen(threadNamePath, "r");
  if (threadNameFile == nullptr) {
    errno = 0;
    return "";
  }

  char threadName[16]{};
  char* res = fgets(threadName, 16, threadNameFile);
  fclose(threadNameFile);
  errno = 0;
  if (res == nullptr) {
      return "";
  }

  return std::string(threadName);
}

TaskStatInfo getStatInfo(uint32_t tid) {
  return TaskStatFile(tid).refresh();
}

ThreadStatInfo::ThreadStatInfo() :
    cpuTimeMs(),
    state(ThreadState::TS_UNKNOWN),
    majorFaults(),
    cpuNum(-1),
    kernelCpuTimeMs(),
    minorFaults(),
    highPrecisionCpuTimeMs(),
    waitToRunTimeMs(),
    nrVoluntarySwitches(),
    nrInvoluntarySwitches(),
    iowaitSum(),
    iowaitCount(),
    availableStatsMask(0) {}

SchedstatInfo::SchedstatInfo() : cpuTimeMs(0), waitToRunTimeMs(0) {}

SchedInfo::SchedInfo()
    : nrVoluntarySwitches(0),
      nrInvoluntarySwitches(0),
      iowaitSum(0),
      iowaitCount(0) {}

TaskStatInfo::TaskStatInfo()
    : cpuTime(0),
      state(ThreadState::TS_UNKNOWN),
      majorFaults(0),
      cpuNum(-1),
      kernelCpuTimeMs(0),
      minorFaults(0) {}

namespace {

enum StatFileType: int8_t {
  STAT      = 0,
  SCHEDSTAT = 1,
  SCHED     = 2,
};

static const std::array<int32_t, 3> kFileStats = {
    /*STAT*/ StatType::CPU_TIME | StatType::STATE | StatType::MAJOR_FAULTS |
        StatType::CPU_NUM | StatType::KERNEL_CPU_TIME | StatType::MINOR_FAULTS,
    /*SCHEDSTAT*/ StatType::HIGH_PRECISION_CPU_TIME |
        StatType::WAIT_TO_RUN_TIME,
    /*SCHED*/ StatType::NR_VOLUNTARY_SWITCHES |
        StatType::NR_INVOLUNTARY_SWITCHES | StatType::IOWAIT_SUM |
        StatType::IOWAIT_COUNT,
};

inline ThreadState convertCharToStateEnum(char stateChar) {
  switch (stateChar) {
    case 'R' : return TS_RUNNING;
    case 'S' : return TS_SLEEPING;
    case 'D' : return TS_WAITING;
    case 'Z' : return TS_ZOMBIE;
    case 'T' : return TS_STOPPED;
    case 't' : return TS_TRACING_STOP;
    case 'X' :
    case 'x' : return TS_DEAD;
    case 'K' : return TS_WAKEKILL;
    case 'W' : return TS_WAKING;
    case 'P' : return TS_PARKED;
  }
  return TS_UNKNOWN;
}

// Consumes data until `ch` or we reach `end`.
// Returns a pointer immediately after `ch`.
//
// Throws std::runtime_error if `end` is reached before `ch`
// or \0 is encountered anywhere in the string.
char* skipUntil(char* data, const char* end, char ch) {

  // It's important that we check against `end`
  // before we dereference `data`.
  while (data < end && *data != ch) {
    if (*data == '\0') {
      throw std::runtime_error("Unexpected end of string");
    }

    ++data;
  }

  if (data == end) {
    throw std::runtime_error("Unexpected end of string");
  }

  // One past the `ch` character.
  return ++data;
}

TaskStatInfo parseStatFile(char* data, size_t size, uint32_t stats_mask) {
  const char* end = (data + size);

  data = skipUntil(data, end, ' '); // pid
  data = skipUntil(data, end, ')'); // name
  data = skipUntil(data, end, ' '); // space after name
  char state = *data; // state

  data = skipUntil(data, end, ' '); // state

  data = skipUntil(data, end, ' '); // ppid
  data = skipUntil(data, end, ' '); // pgrp
  data = skipUntil(data, end, ' '); // session
  data = skipUntil(data, end, ' '); // tty_nr
  data = skipUntil(data, end, ' '); // tpgid

  data = skipUntil(data, end, ' '); // flags

  char* endptr = nullptr;
  long minflt = strtol(data, &endptr, 10); // minflt
  if (errno == ERANGE || data == endptr || endptr > end) {
    throw std::runtime_error("Could not parse minflt");
  }

  data = skipUntil(endptr, end, ' '); // cminflt

  endptr = nullptr;
  long majflt = strtol(data, &endptr, 10); // majflt
  if (errno == ERANGE || data == endptr || endptr > end) {
    throw std::runtime_error("Could not parse majflt");
  }
  data = skipUntil(endptr, end, ' ');

  data = skipUntil(data, end, ' '); // cmajflt

  endptr = nullptr;
  long utime = strtol(data, &endptr, 10); // utime
  if (errno == ERANGE || data == endptr || endptr > end) {
    throw std::runtime_error("Could not parse utime");
  }
  data = skipUntil(endptr, end, ' ');

  endptr = nullptr;
  long stime = strtol(data, &endptr, 10); // stime
  if (errno == ERANGE || data == endptr || endptr > end) {
    throw std::runtime_error("Could not parse stime");
  }

  int cpuNum = 0;
  if (StatType::CPU_NUM & stats_mask) {
    data = skipUntil(endptr, end, ' ');

    data = skipUntil(data, end, ' '); // cutime
    data = skipUntil(data, end, ' '); // cstime
    data = skipUntil(data, end, ' '); // priority
    data = skipUntil(data, end, ' '); // nice
    data = skipUntil(data, end, ' '); // num_threads
    data = skipUntil(data, end, ' '); // itrealvalue
    data = skipUntil(data, end, ' '); // starttime

    data = skipUntil(data, end, ' '); // vsize
    data = skipUntil(data, end, ' '); // rss
    data = skipUntil(data, end, ' '); // rsslim
    data = skipUntil(data, end, ' '); // startcode
    data = skipUntil(data, end, ' '); // endcode
    data = skipUntil(data, end, ' '); // startstack
    data = skipUntil(data, end, ' '); // kstkesp
    data = skipUntil(data, end, ' '); // kstkeip

    data = skipUntil(data, end, ' '); // signal
    data = skipUntil(data, end, ' '); // blocked
    data = skipUntil(data, end, ' '); // sigignore
    data = skipUntil(data, end, ' '); // sigcatch
    data = skipUntil(data, end, ' '); // wchan
    data = skipUntil(data, end, ' '); // nswap
    data = skipUntil(data, end, ' '); // cnswap
    data = skipUntil(data, end, ' '); // exit_signal
    endptr = nullptr;
    cpuNum = strtol(data, &endptr, 10); // processor
    if (errno == ERANGE || data == endptr || endptr > end) {
      throw std::runtime_error("Could not parse cpu num");
    }
  }

  // SYSTEM_CLK_TCK is defined as 100 in linux as is unchanged in android.
  // Therefore there are 10 milli seconds in each clock tick.
  static int kClockTicksMs = systemClockTickIntervalMs();

  TaskStatInfo info{};
  info.cpuTime = kClockTicksMs * (utime + stime);
  info.kernelCpuTimeMs = kClockTicksMs * stime;
  info.state = convertCharToStateEnum(state);
  info.majorFaults = majflt;
  info.minorFaults = minflt;
  info.cpuNum = cpuNum;

  return info;
}

std::string tidToStatPath(uint32_t tid, const char* stat_name) {
  char threadStatPath[kMaxProcFileLength]{};

  int bytesWritten =
    snprintf(
      threadStatPath,
      kMaxProcFileLength,
      "/proc/self/task/%d/%s",
      tid,
      stat_name);

  if (bytesWritten < 0 || bytesWritten >= kMaxProcFileLength) {
    throw std::system_error(
      errno,
      std::system_category(),
      "Could not format file path");
  }
  return std::string(threadStatPath);
}

SchedstatInfo parseSchedstatFile(char* data, size_t size) {
  const char* end = (data + size);

  char* endptr = nullptr;
  int64_t run_time_ns = strtoll(data, &endptr, 10); // run time
  if (errno == ERANGE || data == endptr || endptr > end) {
    throw std::runtime_error("Could not parse run time");
  }
  data = skipUntil(endptr, end, ' ');
  endptr = nullptr;
  long long wait_time_ns = strtoll(data, &endptr, 10); // run time
  if (errno == ERANGE || data == endptr || endptr > end) {
    throw std::runtime_error("Could not parse wait time");
  }

  SchedstatInfo info{};
  info.cpuTimeMs = (long)(run_time_ns / 1000000);
  info.waitToRunTimeMs = (long)(wait_time_ns / 1000000);

  return info;
}

} // namespace

TaskStatFile::TaskStatFile(uint32_t tid)
    : BaseStatFile<TaskStatInfo>(tidToStatPath(tid, "stat")) {}

TaskStatInfo TaskStatFile::doRead(int fd, uint32_t requested_stats_mask) {
  // This is a conservative upper bound, so we can read the
  // entire file in one fread call.
  constexpr size_t kMaxStatFileLength = 512;

  char buffer[kMaxStatFileLength]{};
  int bytes_read = read(fd, buffer, (sizeof(buffer) - 1));
  if (bytes_read < 0) {
    throw std::system_error(
      errno,
      std::system_category(),
      "Could not read stat file");
  }

  // At this point we know that `buffer` must be null terminated because we
  // zeroed the array before we read at most `sizeof(buffer) - 1` from the file.
  return parseStatFile(buffer, bytes_read, requested_stats_mask);
}

TaskSchedstatFile::TaskSchedstatFile(uint32_t tid)
    : BaseStatFile<SchedstatInfo>(tidToStatPath(tid, "schedstat")) {}

SchedstatInfo TaskSchedstatFile::doRead(int fd, uint32_t requested_stats_mask) {
  // This is a conservative upper bound, so we can read the
  // entire file in one fread call.
  constexpr size_t kMaxStatFileLength = 128;

  char buffer[kMaxStatFileLength]{};
  int bytes_read = read(fd, buffer, (sizeof(buffer) - 1));
  if (bytes_read < 0) {
    throw std::system_error(
      errno,
      std::system_category(),
      "Could not read schedstat file");
  }

  // At this point we know that `buffer` must be null terminated because we
  // zeroed the array before we read at most `sizeof(buffer) - 1` from the file.
  return parseSchedstatFile(buffer, bytes_read);
}

TaskSchedFile::TaskSchedFile(uint32_t tid)
  : BaseStatFile<SchedInfo>(tidToStatPath(tid, "sched")),
  value_offsets_(),
  initialized_(false),
  value_size_(),
  availableStatsMask(0) {}

SchedInfo TaskSchedFile::doRead(int fd, uint32_t requested_stats_mask) {
  constexpr size_t kMaxStatLineLength = 4096;
  char buffer[kMaxStatLineLength]{};
  int size = read(fd, buffer, sizeof(buffer) - 1);
  if (size < 0) {
    throw std::system_error(
        errno, std::system_category(), "Could not read stat file");
  }

  if (!initialized_) {
    struct KnownKey {
      const char * key;
      StatType type;
    };

    static std::array<KnownKey, 4> kKnownKeys = {{
      {"nr_voluntary_switches", StatType::NR_VOLUNTARY_SWITCHES},
      {"nr_involuntary_switches", StatType::NR_INVOLUNTARY_SWITCHES},
      {"se.statistics.iowait_count", StatType::IOWAIT_COUNT},
      {"se.statistics.iowait_sum", StatType::IOWAIT_SUM}
    }};

    auto endline = std::strchr(buffer, '\n');
    if (endline == nullptr) {
      throw std::runtime_error("Unexpected file format");
    }

    auto endline_2 = std::strchr(endline + 1, '\n');
    if (endline == nullptr) {
      throw std::runtime_error("Unexpected file format");
    }

    auto line_len = std::distance(endline, endline_2);
    auto pos = endline_2 + 1;

    // The stat file line is in the form of key:value record with fixed line
    // length. The key is aligned to the left and the value is aligned to the
    // right:  "key     :     value"
    // In the loop we parse the buffer line by line reading the key-value pairs.
    // If key is in the known keys (kKnownKeys) we calculate a global offset to
    // the value in the file and record it for fast access in the future.
    for (; pos < buffer + size; pos += line_len) {
      auto key_end = strchr(pos, ' ');
      if (key_end == nullptr) {
        break;
      }
      auto key_len = std::distance(pos, key_end);
      auto known_key = std::find_if(
        kKnownKeys.begin(),
        kKnownKeys.end(),
        [pos, key_len](KnownKey const& key) {
          return std::strncmp(key.key, pos, key_len) == 0;
        });

      if (known_key == kKnownKeys.end()) {
        continue;
      }

      auto delim = strchr(key_end, ':');
      if (delim == nullptr) {
        break;
      }
      int value_offset = std::distance(buffer, delim) + 1;
      if (!value_size_) {
        value_size_ = line_len - 1 - std::distance(pos, delim);
      }
      value_offsets_.push_back(std::make_pair(known_key->type, value_offset));
      availableStatsMask |= known_key->type;
    }

    initialized_ = true;
  }

  if (value_offsets_.empty()) {
    throw std::runtime_error("No target fields found");
  }

  SchedInfo schedInfo{};
  for (auto& entry : value_offsets_) {
    auto key_type = entry.first;
    auto value_offset = entry.second;

    if (value_offset >= size) {
      throw std::runtime_error(
          "Error trying to read value by pre-defined offset");
    }
    char* end_buffer = buffer + size - 1;
    auto value = strtoul(buffer + value_offset, &end_buffer, value_size_);
    if (errno == ERANGE) {
      throw std::runtime_error("Value out of range");
    }

    switch (key_type) {
      case StatType::NR_VOLUNTARY_SWITCHES:
        schedInfo.nrVoluntarySwitches = value;
        break;
      case StatType::NR_INVOLUNTARY_SWITCHES:
        schedInfo.nrInvoluntarySwitches = value;
        break;
      case StatType::IOWAIT_COUNT:
        schedInfo.iowaitCount = value;
        break;
      case StatType::IOWAIT_SUM:
        schedInfo.iowaitSum = value;
        break;
    }
  }

  return schedInfo;
}

ThreadStatHolder::ThreadStatHolder(uint32_t tid) :
  stat_file_(),
  schedstat_file_(),
  sched_file_(),
  last_info_(),
  availableStatFilesMask_(0xff),
  availableStatsMask_(0),
  tid_(tid) {}

ThreadStatInfo ThreadStatHolder::refresh(uint32_t requested_stats_mask) {
  // Assuming that /proc/self/<tid>/stat is always available.
  if (kFileStats[StatFileType::STAT] & requested_stats_mask) {
    if (stat_file_.get() == nullptr) {
      stat_file_ = std::make_unique<TaskStatFile>(tid_);
    }
    auto statInfo = stat_file_->refresh(requested_stats_mask);
    last_info_.cpuTimeMs = statInfo.cpuTime;
    last_info_.state = statInfo.state;
    last_info_.majorFaults = statInfo.majorFaults;
    last_info_.cpuNum = statInfo.cpuNum;
    last_info_.kernelCpuTimeMs = statInfo.kernelCpuTimeMs;
    last_info_.minorFaults = statInfo.minorFaults;
    availableStatsMask_ |=
      kFileStats[StatFileType::STAT] & requested_stats_mask;
  }
  // If /proc/self/<tid>/schedstat is requested, we will try to read it.
  // If we get exception on first read the availableStatFilesMask will be
  // updated respectively. The second time this stat file will be ignored.
  if ((availableStatFilesMask_ & StatFileType::SCHEDSTAT) &&
    (kFileStats[StatFileType::SCHEDSTAT] & requested_stats_mask)) {
    if (schedstat_file_.get() == nullptr) {
      schedstat_file_ = std::make_unique<TaskSchedstatFile>(tid_);
    }
    try {
      auto schedstatInfo = schedstat_file_->refresh(requested_stats_mask);
      last_info_.waitToRunTimeMs = schedstatInfo.waitToRunTimeMs;
      last_info_.highPrecisionCpuTimeMs = schedstatInfo.cpuTimeMs;
      availableStatsMask_ |= kFileStats[StatFileType::SCHEDSTAT];
    } catch (const std::system_error& e) {
      // If 'schedstat' file is absent do not attempt the second time
      availableStatFilesMask_ ^= StatFileType::SCHEDSTAT;
      schedstat_file_.release();
    }
  }
  // If /proc/self/<tid>/sched is requested, we will try to read it.
  // If we get exception on first read the availableStatFilesMask will be
  // updated respectively. The second time this stat file will be ignored.
  if ((availableStatFilesMask_ & StatFileType::SCHED) &&
      (kFileStats[StatFileType::SCHED] & requested_stats_mask)) {
    if (sched_file_.get() == nullptr) {
      sched_file_ = std::make_unique<TaskSchedFile>(tid_);
    }
    try {
      auto schedInfo = sched_file_->refresh(requested_stats_mask);
      last_info_.nrVoluntarySwitches = schedInfo.nrVoluntarySwitches;
      last_info_.nrInvoluntarySwitches = schedInfo.nrInvoluntarySwitches;
      last_info_.iowaitSum = schedInfo.iowaitSum;
      last_info_.iowaitCount = schedInfo.iowaitCount;
      availableStatsMask_ |= sched_file_->availableStatsMask;
    } catch (const std::exception& e) {
      // If 'schedstat' file is absent do not attempt the second time
      availableStatFilesMask_ ^= StatFileType::SCHED;
      schedstat_file_.release();
    }
  }
  last_info_.availableStatsMask = availableStatsMask_;
  return last_info_;
}

ThreadStatInfo ThreadStatHolder::getInfo() {
  return last_info_;
}

void ThreadCache::forEach(
  stats_callback_fn callback,
  uint32_t requested_stats_mask) {
  try {
    const auto& threads = util::threadListFromProcFs();

    // Delete cached data for gone threads.
    for (auto iter = cache_.begin(); iter != cache_.end();) {
      if (threads.find(iter->first) == threads.end()) {
        iter = cache_.erase(iter);
      } else {
        ++iter;
      }
    }

    for (auto tid : threads) {
      forThread(tid, callback, requested_stats_mask);
    }
  } catch (const std::system_error& e) {
    // threadListFromProcFs can throw an error. Ignore it.
    return;
  }
}

void ThreadCache::forThread(
  uint32_t tid,
  stats_callback_fn callback,
  uint32_t requested_stats_mask) {

  auto statIter = cache_.find(tid);
  if (statIter == cache_.end()) {
    cache_.emplace(std::make_pair(tid, ThreadStatHolder(tid)));
  }
  auto& statHolder = cache_.at(tid);

  auto prevInfo = statHolder.getInfo();
  util::ThreadStatInfo currInfo;

  try {
    currInfo = statHolder.refresh(requested_stats_mask);
  } catch (const std::system_error& e) {
    return;
  } catch (const std::runtime_error& e) {
    return;
  }

  /*
  Do refresh and staff right here
  */
  callback(tid, prevInfo, currInfo);
}

ThreadStatInfo ThreadCache::getRecentStats(int32_t tid) {
  if (getStatsAvailabililty(tid) == 0) {
    throw new std::runtime_error("Cache is empty");
  }
  return cache_.at(tid).getInfo();
}

int32_t ThreadCache::getStatsAvailabililty(int32_t tid) {
  int32_t stats_mask = 0;
  if (cache_.find(tid) != cache_.end()) {
      stats_mask = cache_.at(tid).getInfo().availableStatsMask;
  }
  return stats_mask;
}

void ThreadCache::clear() {
  cache_.clear();
}

} // util
} // profilo
} // facebook
