// Copyright 2004-present Facebook. All Rights Reserved.

#include <fbjni/fbjni.h>

#include "TraceProviders.h"

namespace facebook {
namespace profilo {

TraceProviders& TraceProviders::get() {
  static TraceProviders providers{};
  return providers;
}

bool TraceProviders::isEnabled(const std::string& provider) {
  // The native side doesn't have the full name -> int mapping.
  // This function retrieves the mapping for a single provider and caches it, so future
  // accesses don't have to cross the JNI boundary.

  static auto cls = jni::findClassStatic("com/facebook/profilo/core/ProvidersRegistry");
  static auto getBitMaskMethod = cls->getStaticMethod<jint(std::string)>("getBitMaskFor");

  {
    // Reader side of the lock only, this is the fast path.
    std::shared_lock<std::shared_timed_mutex> lock(name_lookup_mutex_);
    auto iter = name_lookup_cache_.find(provider);
    if (iter != name_lookup_cache_.end()) {
      return isEnabled(iter->second);
    }
  }

  {
    // Cache miss, need to ask Java and cache here.
    auto bitmask = getBitMaskMethod(cls, provider);

    std::unique_lock<std::shared_timed_mutex> lock(name_lookup_mutex_);
    name_lookup_cache_.emplace(provider, bitmask);
    return isEnabled(bitmask);
  }
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

} // namespace profilo
} // namespace facebook
