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

#include "ExternalTracer.h"

namespace facebook {
namespace profilo {
namespace profiler {

/*static*/ std::mutex& ExternalTracer::getLock() {
  static std::mutex lock;
  return lock;
}

/*static*/ std::unordered_map<int32_t, std::shared_ptr<ExternalTracer>>&
ExternalTracer::getExternalTracers() {
  static std::unordered_map<int32_t, std::shared_ptr<ExternalTracer>>
      externalTracers;
  return externalTracers;
}

/*static*/ std::unordered_map<int32_t, profilo_int_collect_stack_fn>&
ExternalTracer::getPendingRegistrations() {
  static std::unordered_map<int32_t, profilo_int_collect_stack_fn>
      pendingRegistrations;
  return pendingRegistrations;
}

ExternalTracer::ExternalTracer(int32_t tracerType) {
  std::lock_guard<std::mutex> lockGuard(getLock());
  auto& externalTracers = getExternalTracers();
  assert(
      externalTracers.find(tracerType) == externalTracers.end() &&
      "Tracer has been registered.");
  externalTracers[tracerType] = shared_from_this();
  registerPendingCallbackLocked(tracerType);
}

void ExternalTracer::registerPendingCallbackLocked(int32_t tracerType) {
  auto& pendingRegistrations = getPendingRegistrations();
  auto iter = pendingRegistrations.find(tracerType);
  if (iter != pendingRegistrations.end()) {
    this->callback_.store(iter->second, std::memory_order_release);
    pendingRegistrations.erase(iter);
  }
}

bool ExternalTracer::collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint8_t& depth,
    uint8_t max_depth) {
  auto fn = callback_.load(std::memory_order_acquire);
  return fn != nullptr && fn(ucontext, frames, &depth, max_depth);
}

/*static*/ bool ExternalTracer::registerCallback(
    int32_t tracerType,
    profilo_int_collect_stack_fn callback) {
  std::lock_guard<std::mutex> lockGuard(getLock());
  auto& externalTracers = getExternalTracers();
  auto& pendingRegistrations = getPendingRegistrations();
  if (externalTracers.find(tracerType) == externalTracers.end()) {
    pendingRegistrations[tracerType] = callback;
  } else {
    externalTracers[tracerType]->callback_.store(
        callback, std::memory_order_release);
  }
  return true;
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
