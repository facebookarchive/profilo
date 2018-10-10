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

#include "profiler/ExternalTracer.h"

#include <profilo/ExternalApi.h>

#include <unistd.h>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace facebook {
namespace profilo {
namespace profiler {

/// Singleton class that manages all external tracers for external
/// callback registration.
class ExternalTracerManager {
 private:
  /// Protect externalTracers_ and pendingRegistrations_.
  std::mutex lock_;

  /// A map of available external tracers keyed by its tracer type.
  std::unordered_map<int32_t, std::shared_ptr<ExternalTracer>> externalTracers_;

  /// A map of pending external tracer callbacks keyed by its tracer
  /// type. This is needed because client may register its callback before
  /// profiler initialized so we need to keep all the callbacks and register
  /// when all tracers are available.
  std::unordered_map<int32_t, profilo_int_collect_stack_fn>
      pendingRegistrations_;

  /// Register any pending external callback of \p tracerType for \p tracer.
  /// Caller should have taken the lock before calling.
  void registerPendingCallbackLocked(
      int32_t tracerType,
      const std::shared_ptr<ExternalTracer>& tracer);

  /// Prevent creation.
  explicit ExternalTracerManager() = default;

 public:
  /// \return the singleton manager instance.
  static ExternalTracerManager& getInstance();

  /// Register an external tracer.
  void registerExternalTracer(const std::shared_ptr<ExternalTracer>& tracer);

  /// Register external \p callback for \p tracerType.
  bool registerCallback(
      int32_t tracerType,
      profilo_int_collect_stack_fn callback);
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
