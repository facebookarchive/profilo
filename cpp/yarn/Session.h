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

#include <time.h>
#include <unistd.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include <yarn/Event.h>
#include <yarn/detail/AttachmentStrategy.h>
#include <yarn/detail/Reader.h>
#include <yarn/detail/make_unique.h>

namespace facebook {
namespace yarn {

enum FallbackMode {
  FALLBACK_RAISE_RLIMIT = 1
  // FALLBACK_NO_FDS = 2 // planned support for memory polling and releasing fds
};

struct SessionSpec {
  const uint32_t fallbacks;

  // How many times to try to attach the events before giving up
  const uint16_t maxAttachIterations;

  // How many file descriptors are allowed to stay around after attachment,
  // as a proportion of the overall limit ([0, 1.0] range)
  const float maxAttachedFdsRatio;
};

class Session {
 public:
  explicit Session(
      const std::vector<EventSpec>& events,
      const SessionSpec& spec,
      std::unique_ptr<RecordListener> listener = nullptr);

  Session(Session& session) = delete;
  void operator=(Session& session) = delete;

  //
  // Attach the specified events to the current process, obeying the
  // SessionSpec from the constructor parameters.
  //
  // Returns true if the session attached *fully*, false otherwise (and
  // partial attachment is reverted).
  //
  bool attach();

  // The caller must ensure thread safety for this call. In particular, it's
  // not safe to call this if another thread is currently in a read() call.
  void detach();

  // Enter the reading loop. This function will return only after a call to
  // stopRead().
  void read();

  // Request that the current read() execution is stopped. Callable from any
  // thread. Calling this has no effect if read() is not concurrently running.
  // This call returns when read() is no longer reading any events.
  void stopRead();

 private:
  const std::vector<EventSpec> events_;
  const SessionSpec spec_;

  EventList perf_events_;
  std::unique_ptr<detail::Reader> reader_;
  std::unique_ptr<RecordListener> listener_;
};
} // namespace yarn
} // namespace facebook
