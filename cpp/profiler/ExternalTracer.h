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

#include "profiler/BaseTracer.h"

#include <profilo/ExternalApi.h>

#include <assert.h>
#include <unistd.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace facebook {
namespace profilo {
namespace profiler {

/// Base class for all external tracers.
/// External tracer allows an external component to register its own
/// collect stack callback.
class ExternalTracer : public std::enable_shared_from_this<ExternalTracer>,
                       public BaseTracer {
 private:
  /// External callback for this tracer.
  std::atomic<profilo_int_collect_stack_fn> callback_{nullptr};

 private:
  /// Protect externalTracers and pendingRegistrations.
  static std::mutex& getLock();

  /// Return a map of available external tracers keyed by its tracer type.
  static std::unordered_map<int32_t, std::shared_ptr<ExternalTracer>>&
  getExternalTracers();

  /// Return a map of pending external tracer callbacks keyed by its tracer
  /// type. This is needed because client may register its callback before
  /// profiler initialized so we need to keep all the callbacks and register
  /// when all tracers are available.
  static std::unordered_map<int32_t, profilo_int_collect_stack_fn>&
  getPendingRegistrations();

  /// Register any pending external callback of \p tracerType.
  /// Caller should have taken the lock before calling.
  void registerPendingCallbackLocked(int32_t tracerType);

 public:
  explicit ExternalTracer(int32_t tracerType);

  /// Register external \p callback for \p tracerType.
  static bool registerCallback(
      int32_t tracerType,
      profilo_int_collect_stack_fn callback);

  bool collectStack(
      ucontext_t* ucontext,
      int64_t* frames,
      uint8_t& depth,
      uint8_t max_depth) override;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
