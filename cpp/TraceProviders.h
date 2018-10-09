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
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace facebook {
namespace profilo {

class TraceProviders {
 public:
  static TraceProviders& get();

  bool isEnabled(const std::string& provider);

  inline bool isEnabled(uint32_t providers) {
    return enabledMask(providers) == providers;
  }

  inline uint32_t enabledMask(uint32_t providers) {
    // memory_order_relaxed because we have a time-of-check-time-of-use race
    // anyway (between the isEnabled call and the actual work the writer
    // wants to do), so might as well be gentle with the memory barriers.
    return providers_.load(std::memory_order_relaxed) & providers;
  }

  uint32_t enableProviders(uint32_t providers);
  uint32_t disableProviders(uint32_t providers);
  void clearAllProviders();
  void initProviderNames(
      std::unordered_map<std::string, uint32_t>&& provider_names);

 private:
  TraceProviders() : mutex_(), providers_(0), provider_counts_({}) {}
  std::mutex mutex_; // Guards providers modifications
  std::atomic<uint32_t> providers_;
  std::array<uint8_t, 32> provider_counts_;

  std::shared_timed_mutex name_lookup_mutex_; // Guards provider name lookup.
  std::unordered_map<std::string, uint32_t> name_lookup_cache_;
};

} // namespace profilo
} // namespace facebook
