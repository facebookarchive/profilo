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

#include <util/SysFs.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdio>
#include <map>
#include <system_error>

namespace facebook {
namespace profilo {
namespace util {

namespace {

static constexpr int kMaxSysPathLength = 64;

std::string getCpuStatFilePath(int cpu, std::string path_format) {
  char freqStatPath[kMaxSysPathLength]{0};
  int bytesWritten =
      snprintf(freqStatPath, kMaxSysPathLength, path_format.c_str(), cpu);

  if (bytesWritten < 0 || bytesWritten >= kMaxSysPathLength) {
    throw std::system_error(
        errno, std::system_category(), "Could not format file path");
  }

  return std::string(freqStatPath);
}

std::string getScalingCurrentCpuFrequencyPath(int cpu) {
  return getCpuStatFilePath(
      cpu, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq");
}

std::string getMaxCpuFrequencyPath(int cpu) {
  return getCpuStatFilePath(
      cpu, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq");
}

long readScalingCurrentFrequency(int fd) {
  char buffer[16]{};
  int bytes_read = read(fd, buffer, (sizeof(buffer) - 1));
  if (bytes_read < 0) {
    throw std::runtime_error("Cannot read current frequency");
  }

  return strtol(buffer, nullptr, 10);
}

long readMaxCpuFrequency(int cpu) {
  int fd = open(getMaxCpuFrequencyPath(cpu).c_str(), O_RDONLY);
  if (fd == -1) {
    throw std::runtime_error("Cannot open max frequency stat file");
  }

  char buffer[16]{};
  int bytes_read = read(fd, buffer, (sizeof(buffer) - 1));
  close(fd);
  if (bytes_read < 0) {
    throw std::runtime_error("Cannot read max frequency");
  }

  return strtol(buffer, nullptr, 10);
}

} // namespace

CpuCurrentFrequencyStatFile::CpuCurrentFrequencyStatFile(int cpu)
    : BaseStatFile(getScalingCurrentCpuFrequencyPath(cpu)) {}

CpuFrequency CpuCurrentFrequencyStatFile::doRead(
    int fd,
    uint32_t requested_stats_mask) {
  return readScalingCurrentFrequency(fd);
}

int64_t CpuFrequencyStats::getCachedCpuFrequency(int8_t cpu) {
  return cache_.at(cpu);
}

int64_t CpuFrequencyStats::getMaxCpuFrequency(int8_t cpu) {
  auto max_frequency = max_cpu_freq_.at(cpu);
  if (max_frequency == 0) {
    max_frequency = readMaxCpuFrequency(cpu);
    max_cpu_freq_[cpu] = max_frequency;
  }
  return max_frequency;
}

int64_t CpuFrequencyStats::refresh(int8_t cpu) {
  auto& cpu_freq_file = cpu_freq_files_.at(cpu);
  if (cpu_freq_file == nullptr) {
    cpu_freq_files_[cpu].reset(new CpuCurrentFrequencyStatFile(cpu));
  }
  auto cur_frequency = cpu_freq_files_[cpu]->refresh();
  cache_[cpu] = cur_frequency;
  return cur_frequency;
}

} // namespace util
} // namespace profilo
} // namespace facebook
