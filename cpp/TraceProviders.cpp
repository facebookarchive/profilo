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

#include "TraceProviders.h"

#include <fbjni/fbjni.h>

#include <algorithm>
#include <cstring>

namespace facebook {
namespace profilo {

namespace {
struct StrCompare {
  bool operator()(const ProviderEntry& entry, const char* key) {
    return strcmp(entry.first.c_str(), key) < 0;
  }

  bool operator()(const char* key, const ProviderEntry& entry) {
    return strcmp(key, entry.first.c_str()) < 0;
  }
};
} // namespace

TraceProviders& TraceProviders::get() {
  static TraceProviders providers{};
  return providers;
}

bool TraceProviders::isEnabled(const char* provider) {
  // The native side doesn't have the full name -> int mapping.
  // This function retrieves the mapping for a single provider from
  // pre-initialized cache.

  // Reader side of the lock only, this is the fast path.
  std::shared_lock<std::shared_timed_mutex> lock(name_lookup_mutex_);
  if (name_lookup_cache_.empty()) {
    return false;
  }
  auto iter = std::lower_bound(
      name_lookup_cache_.begin(),
      name_lookup_cache_.end(),
      provider,
      StrCompare());
  if (iter == name_lookup_cache_.end() ||
      strcmp(iter->first.c_str(), provider) != 0) {
    return false;
  }
  return isEnabled(iter->second);
}

bool TraceProviders::isEnabled(const std::string& provider) {
  return isEnabled(provider.c_str());
}

uint32_t TraceProviders::enableProviders(uint32_t providers) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto p = providers;
  int lsb_index;
  while ((lsb_index = __builtin_ffs(p) - 1) != -1) {
    provider_counts_[lsb_index]++;
    p ^= static_cast<unsigned int>(1) << lsb_index;
  }
  providers_ |= providers;

  return providers_;
}

uint32_t TraceProviders::disableProviders(uint32_t providers) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t disable_providers = 0;
  auto p = providers;
  int lsb_index;
  while ((lsb_index = __builtin_ffs(p) - 1) != -1) {
    if (provider_counts_[lsb_index] > 0) {
      provider_counts_[lsb_index]--;
      if (provider_counts_[lsb_index] == 0) {
        disable_providers |= static_cast<unsigned int>(1) << lsb_index;
      }
    }
    p ^= static_cast<unsigned int>(1) << lsb_index;
  }
  providers_ ^= disable_providers;
  return providers_;
}

void TraceProviders::clearAllProviders() {
  std::lock_guard<std::mutex> lock(mutex_);
  provider_counts_.fill(0);
  providers_ = 0;
}

void TraceProviders::initProviderNames(
    std::vector<ProviderEntry>&& provider_names) {
  std::unique_lock<std::shared_timed_mutex> lock(name_lookup_mutex_);
  std::sort(provider_names.begin(), provider_names.end());
  name_lookup_cache_ = std::move(provider_names);
}

} // namespace profilo
} // namespace facebook
