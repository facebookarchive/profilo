// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <util/BaseStatFile.h>

#include <map>
#include <memory>
#include <vector>
#include <tuple>

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
