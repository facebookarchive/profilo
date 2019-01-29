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

#include "SystemCounterThread.h"

#include <profilo/Logger.h>
#include <util/common.h>

#include <unordered_set>

using facebook::jni::alias_ref;
using facebook::jni::local_ref;

namespace facebook {
namespace profilo {

namespace {

struct Whitelist {
  // When in high freq counter tracing mode, we can optionally whitelist
  // additional threads to profile as well. This list maintains current
  // list of threads that are candidates to be profiled in high freq mode.
  std::unordered_set<int32_t> whitelistedThreads;
  // Since this list is subject to updates by multiple threads, thread
  // safety is important, this following guards whitelistedThreads.
  std::mutex whitelistMtx;
};

// Function wrapper around the static profile state to avoid
// using a DSO constructor.
Whitelist& getWhitelistState() {
  static Whitelist state;
  return state;
}

void addToWhitelist(alias_ref<jclass>, int targetThread) {
  auto& whitelistState = getWhitelistState();
  std::unique_lock<std::mutex> lock(whitelistState.whitelistMtx);
  whitelistState.whitelistedThreads.insert(static_cast<int32_t>(targetThread));
}

void removeFromWhitelist(alias_ref<jclass>, int targetThread) {
  // Don't remove the thread from whitelist if it is the main thread
  static auto pid = getpid();
  if (targetThread == pid) {
    return;
  }
  auto& whitelistState = getWhitelistState();
  std::unique_lock<std::mutex> lock(whitelistState.whitelistMtx);
  whitelistState.whitelistedThreads.erase(targetThread);
}

} // namespace

local_ref<SystemCounterThread::jhybriddata> SystemCounterThread::initHybrid(
    alias_ref<jclass>) {
  return makeCxxInstance();
}

void SystemCounterThread::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", SystemCounterThread::initHybrid),
      makeNativeMethod("logCounters", SystemCounterThread::logCounters),
      makeNativeMethod(
          "logHighFrequencyThreadCounters",
          SystemCounterThread::logHighFrequencyThreadCounters),
      makeNativeMethod(
          "logTraceAnnotations", SystemCounterThread::logTraceAnnotations),
      makeNativeMethod("nativeAddToWhitelist", addToWhitelist),
      makeNativeMethod("nativeRemoveFromWhitelist", removeFromWhitelist),
      makeNativeMethod(
          "nativeSetHighFrequencyMode",
          SystemCounterThread::setHighFrequencyMode),
  });
}

void SystemCounterThread::logCounters() {

  // When collecting counters for all threads and in high frequency mode then
  // thread ids from the high frequency whitelist should be ignored.
  // Making a copy of whitelist here to avoid holding the whitelist lock while
  // collecting counter data.
  std::unordered_set<int32_t> ignoredTids;
  if (highFrequencyMode_) {
    auto& whitelistState = getWhitelistState();
    std::unique_lock<std::mutex> lockT(whitelistState.whitelistMtx);
    ignoredTids = whitelistState.whitelistedThreads;
  }
  threadCounters_.logCounters(highFrequencyMode_, ignoredTids);

  processCounters_.logCounters();
  systemCounters_.logCounters();
}

void SystemCounterThread::logHighFrequencyThreadCounters() {
  std::unordered_set<int32_t> whitelist;
  auto& whitelistState = getWhitelistState();
  {
    std::unique_lock<std::mutex> lockT(whitelistState.whitelistMtx);
    whitelist = whitelistState.whitelistedThreads;
  }
  threadCounters_.logHighFreqCounters(whitelist);
  systemCounters_.logHighFreqCounters();
}

void SystemCounterThread::logTraceAnnotations() {
  Logger::get().writeTraceAnnotation(
      QuickLogConstants::AVAILABLE_COUNTERS,
      processCounters_.getAvailableCounters() |
          systemCounters_.getAvailableCounters() |
          threadCounters_.getAvailableCounters());
}

} // namespace profilo
} // namespace facebook
