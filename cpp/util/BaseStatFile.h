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
#include <cerrno>
#include <string>
#include <system_error>

namespace facebook {
namespace profilo {
namespace util {

enum StatType : int32_t {
  CPU_TIME = 1,
  STATE = 1 << 1,
  MAJOR_FAULTS = 1 << 2,
  HIGH_PRECISION_CPU_TIME = 1 << 3,
  WAIT_TO_RUN_TIME = 1 << 4,
  NR_VOLUNTARY_SWITCHES = 1 << 5,
  NR_INVOLUNTARY_SWITCHES = 1 << 6,
  IOWAIT_SUM = 1 << 7,
  IOWAIT_COUNT = 1 << 8,
  CPU_NUM = 1 << 9,
  CPU_FREQ = 1 << 10,
  MINOR_FAULTS = 1 << 11,
  KERNEL_CPU_TIME = 1 << 12,
  VMSTAT_NR_FREE_PAGES = 1 << 13,
  VMSTAT_NR_DIRTY = 1 << 14,
  VMSTAT_NR_WRITEBACK = 1 << 15,
  VMSTAT_PGPGIN = 1 << 16,
  VMSTAT_PGPGOUT = 1 << 17,
  VMSTAT_PGMAJFAULT = 1 << 18,
  VMSTAT_ALLOCSTALL = 1 << 19,
  VMSTAT_PAGEOUTRUN = 1 << 20,
  VMSTAT_KSWAPD_STEAL = 1 << 21,
};

template <class StatInfo>
class BaseStatFile {
 public:
  explicit BaseStatFile(std::string path)
      : path_(path), fd_(-1), last_info_() {}

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
  StatInfo refresh(uint32_t requested_stats_mask = 0) {
    if (fd_ == -1) {
      fd_ = doOpen(path_);
    }

    if (lseek(fd_, 0, SEEK_SET)) {
      throw std::system_error(
          errno, std::system_category(), "Could not rewind file");
    }

    last_info_ = doRead(fd_, requested_stats_mask);
    return last_info_;
  }

  int doOpen(const std::string& path) {
    int statFile = open(path.c_str(), O_SYNC | O_RDONLY);

    if (statFile == -1) {
      throw std::system_error(
          errno, std::system_category(), "Could not open stat file");
    }
    return statFile;
  }

 protected:
  virtual StatInfo doRead(int fd, uint32_t requested_stats_mask) = 0;

 private:
  std::string path_;
  int fd_;
  StatInfo last_info_;
};

} // namespace util
} // namespace profilo
} // namespace facebook
