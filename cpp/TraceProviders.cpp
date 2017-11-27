// Copyright 2004-present Facebook. All Rights Reserved.

#include "TraceProviders.h"

namespace facebook {
namespace loom {

TraceProviders& TraceProviders::get() {
  static TraceProviders providers{};
  return providers;
}

void TraceProviders::enableProviders(uint32_t providers) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto p = providers;
  int lsb_index;
  while ((lsb_index = __builtin_ffs(p) -1) != -1) {
    provider_counts_[lsb_index]++;
    p ^= static_cast<unsigned int>(1) << lsb_index;
  }
  providers_ |= providers;
}

void TraceProviders::disableProviders(uint32_t providers) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t disable_providers = 0;
  auto p = providers;
  int lsb_index;
  while ((lsb_index = __builtin_ffs(p) -1) != -1) {
    if (provider_counts_[lsb_index] > 0) {
      provider_counts_[lsb_index]--;
      if (provider_counts_[lsb_index] == 0) {
        disable_providers |= static_cast<unsigned int>(1) << lsb_index;
      }
    }
    p ^= static_cast<unsigned int>(1) << lsb_index;
  }
  providers_ ^= disable_providers;
}

void TraceProviders::clearAllProviders() {
  std::lock_guard<std::mutex> lock(mutex_);
  provider_counts_.fill(0);
  providers_ = 0;
}

} // namespace loom
} // namespace facebook
