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
/// All sub-class should register itself with ExternalTracerManager after
/// creation to participate in the callback registration.
class ExternalTracer : public BaseTracer {
 private:
  /// External callback for this tracer.
  std::atomic<profilo_int_collect_stack_fn> callback_{nullptr};
  /// Type of the tracer.
  int32_t tracerType_;

 public:
  explicit ExternalTracer(int32_t tracerType) : tracerType_(tracerType) {}

  int32_t getType() const {
    return tracerType_;
  }

  /// Register external \p callback for this external tracer.
  void registerCallback(profilo_int_collect_stack_fn callback);

  bool collectStack(
      ucontext_t* ucontext,
      int64_t* frames,
      uint8_t& depth,
      uint8_t max_depth) override;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
