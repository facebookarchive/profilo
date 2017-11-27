// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <array>
#include <atomic>
#include <mutex>

namespace facebook {
namespace loom {

class TraceProviders {
  public:
    static TraceProviders& get();

    inline bool isEnabled(uint32_t provider) {
      // memory_order_relaxed because we have a time-of-check-time-of-use race
      // anyway (between the isEnabled call and the actual work the writer
      // wants to do), so might as well be gentle with the memory barriers.
      return (providers_.load(std::memory_order_relaxed) & provider) == provider;
    }

    inline uint32_t enabledMask(uint32_t providers) {
      return providers_.load(std::memory_order_relaxed) & providers;
    }

    void enableProviders(uint32_t providers);
    void disableProviders(uint32_t providers);
    void clearAllProviders();

  private:
    TraceProviders(): mutex_(), providers_(0), provider_counts_({}){}
    std::mutex mutex_; // Guards providers modifications
    std::atomic<uint32_t> providers_;
    std::array<uint8_t, 32> provider_counts_;
};

} // namespace loom
} // namespace facebook
