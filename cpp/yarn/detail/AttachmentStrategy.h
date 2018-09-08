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

#include <unistd.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <system_error>
#include <vector>

#include <yarn/Event.h>
#include <yarn/Session.h>
#include <yarn/detail/RLimits.h>
#include <yarn/detail/make_unique.h>

#include <util/ProcFs.h>

namespace facebook {
namespace yarn {
namespace detail {

using namespace facebook::profilo::util;

using EventList = std::vector<Event>;

//
// An AttachmentStrategy describes how to convert a set of EventSpecs into
// actual Events.
//
class AttachmentStrategy {
 public:
  //
  // Returns a list of open (and potentially mapped) Events or an
  // empty list if attachment failed.
  //
  // May throw std::system_error if a system call returned an unexpected error.
  //
  virtual EventList attach() = 0;
};

//
// PerCoreAttachmentStrategy uses the following behaviors of the
// perf_event_open API:
//
// 1) Event inheritance only works with per-core events. (event inheritance
// propagates events from a thread to every thread spawned from it)
// 2) When using per-core events, event forwarding to a different memory
// buffer works as long as the child event remains open.
// 3) When polling the open file descriptors, polling the "parents" is
// sufficient, changes to the children also trigger the parent.
//
// Thus, this Strategy creates a per-core per-thread event and uses a
// converge-until-fixed-point loop to attach to existing threads with
// inherit = 1. That way, if we reach the fixed point, all future threads will
// be automatically attached via the in-kernel inheritance mechanism.
//
// Of all these events, this strategy only mmaps the first event on
// every core and redirects all other events on that core to this
// first buffer.
//
class PerCoreAttachmentStrategy : public AttachmentStrategy {
 public:
  PerCoreAttachmentStrategy(
      const EventSpecList& specs,
      uint32_t fallbacks = 0,
      uint16_t max_iterations = 1,
      float open_fds_limit_ratio = 1.0f);

  virtual EventList attach();

 private:
  EventSpecList specs_;
  size_t global_specs_;
  uint32_t fallbacks_;
  uint32_t used_fallbacks_;
  uint16_t max_iterations_;
  float open_fds_limit_ratio_;

  bool isWithinLimits(size_t tids_count);
  bool tryFallbacks();

  EventList eventsForDelta(const ThreadList& prev_tids, const ThreadList& tids)
      const;
};

} // namespace detail
} // namespace yarn
} // namespace facebook
