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

#include <poll.h>
#include <sys/eventfd.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <yarn/Event.h>
#include <yarn/Records.h>
#include <yarn/detail/BufferParser.h>
#include <yarn/detail/make_unique.h>

namespace facebook {
namespace yarn {
namespace detail {

class Reader {
 public:
  // Enter the run loop. This function will return only after a call to stop().
  virtual void run() = 0;

  // Request that the current run() execution. Callable from any thread.
  //
  // Calling this has no effect if read() is not concurrently running.
  // This call returns when run() is no longer reading events.
  virtual void stop() = 0;
  virtual ~Reader() = default;
};

//
// This Reader will only read Events that have their buffer mmapped. It puts
// them in a poll(2) set, along with a special eventfd (see eventfd(2)) used
// for safe cross-thread signalling that the Reader should stop.
//
class FdPollReader : public Reader {
 public:
  // Construct a new reader. Will only poll events that
  // already have a mapped buffer.
  FdPollReader(EventList& events, RecordListener* listener = nullptr);

  FdPollReader(FdPollReader& other) = delete;
  virtual ~FdPollReader();

  virtual void run();
  virtual void stop();

 private:
  int stop_fd_;
  EventList& events_;
  std::unordered_map<uint64_t, const Event&> id_event_map_;
  RecordListener* listener_;

  bool running_;
  std::condition_variable running_cv_;
  std::mutex running_mutex_;

  // Returns the value of the stop eventfd counter.
  uint64_t stopValue() const;
};

} // namespace detail
} // namespace yarn
} // namespace facebook
