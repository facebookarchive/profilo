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

#include "ExternalTracerManager.h"

namespace facebook {
namespace profilo {
namespace profiler {

/*static*/ ExternalTracerManager& ExternalTracerManager::getInstance() {
  static ExternalTracerManager instance;
  return instance;
}

void ExternalTracerManager::registerExternalTracer(
    const std::shared_ptr<ExternalTracer>& tracer) {
  std::lock_guard<std::mutex> lockGuard(lock_);
  auto tracerType = tracer->getType();
  assert(
      externalTracers_.find(tracerType) == externalTracers_.end() &&
      "Tracer has been registered.");
  externalTracers_[tracerType] = tracer;
  registerPendingCallbackLocked(tracerType, tracer);
}

void ExternalTracerManager::registerPendingCallbackLocked(
    int32_t tracerType,
    const std::shared_ptr<ExternalTracer>& tracer) {
  auto iter = pendingRegistrations_.find(tracerType);
  if (iter != pendingRegistrations_.end()) {
    tracer->registerCallback(iter->second);
    pendingRegistrations_.erase(iter);
  }
}

bool ExternalTracerManager::registerCallback(
    int32_t tracerType,
    profilo_int_collect_stack_fn callback) {
  std::lock_guard<std::mutex> lockGuard(lock_);
  if (externalTracers_.find(tracerType) == externalTracers_.end()) {
    pendingRegistrations_[tracerType] = callback;
  } else {
    externalTracers_[tracerType]->registerCallback(callback);
  }
  return true;
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
