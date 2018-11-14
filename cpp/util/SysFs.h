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

#include <util/BaseStatFile.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace facebook {
namespace profilo {
namespace util {

typedef int64_t CpuFrequency;

class CpuCurrentFrequencyStatFile : public BaseStatFile<CpuFrequency> {
 public:
  explicit CpuCurrentFrequencyStatFile(int cpu);

  CpuFrequency doRead(int fd, uint32_t requested_stats_mask) override;
};

class CpuFrequencyStats {
 public:
  CpuFrequencyStats(int8_t cores)
      : max_cpu_freq_(cores), cpu_freq_files_(cores), cache_(cores) {}

  int64_t getCachedCpuFrequency(int8_t cpu);
  int64_t getMaxCpuFrequency(int8_t cpu);
  int64_t refresh(int8_t cpu);

 private:
  std::vector<int64_t> max_cpu_freq_;
  std::vector<std::unique_ptr<CpuCurrentFrequencyStatFile>> cpu_freq_files_;
  std::vector<int64_t> cache_;
};

} // namespace util
} // namespace profilo
} // namespace facebook
