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

#include <yarn/detail/Reader.h>

namespace facebook {
namespace yarn {
namespace detail {

struct PollSet {
  std::unique_ptr<std::vector<pollfd>> pollfds;
  std::unique_ptr<std::vector<Event*>> events;
};

//
// The pollfds vector in the result consists of:
// 1) one pollfd for every Event with buffer() != nullptr. The events vector
// variable holds the Event* to the corresponding Event.
//
// 2) a pollfd (last element) for the stopfd that's passed in. The events
// vector holds nullptr for this pollfd.
//
static std::unique_ptr<PollSet> createPollSet(EventList& events, int stopfd) {
  size_t buffer_events = 0;
  for (auto& event : events) {
    if (event.buffer() != nullptr) {
      ++buffer_events;
    }
  }

  size_t pollfd_size = buffer_events + 1; // +1 for the stopfd
  auto poll_set = detail::make_unique<std::vector<pollfd>>(pollfd_size);
  auto event_ptrs = detail::make_unique<std::vector<Event*>>(pollfd_size);

  size_t poll_set_idx = 0;
  for (auto& event : events) {
    if (event.buffer() != nullptr) {
      if (event.fd() == -1) {
        throw std::invalid_argument("Event is mapped but no longer open");
      }
      pollfd pfd{};
      pfd.fd = event.fd();
      pfd.events = POLLIN;

      (*poll_set)[poll_set_idx] = pfd;

      // Not exactly safe. The alternative is to wrap every Event in a
      // shared_ptr and that's quite expensive.
      (*event_ptrs)[poll_set_idx] = &event;
      ++poll_set_idx;
    }
  }

  pollfd pfd{};
  pfd.fd = stopfd;
  pfd.events = POLLIN;
  (*poll_set)[poll_set_idx] = pfd;
  (*event_ptrs)[poll_set_idx] = nullptr;

  PollSet ret{};
  ret.pollfds = std::move(poll_set);
  ret.events = std::move(event_ptrs);

  return detail::make_unique<PollSet>(std::move(ret));
}

static std::unordered_map<uint64_t, const Event&> createIdEventMap(
    EventList& events) {
  auto id_event_map = std::unordered_map<uint64_t, const Event&>();
  for (auto& event : events) {
    id_event_map.emplace(event.id(), event);
  }
  return id_event_map;
}

FdPollReader::FdPollReader(EventList& events, RecordListener* listener)
    : stop_fd_(eventfd(0, EFD_NONBLOCK)),
      events_(events),
      id_event_map_(createIdEventMap(events)),
      listener_(listener),
      running_(false),
      running_cv_(),
      running_mutex_() {}

FdPollReader::~FdPollReader() {
  close(stop_fd_); // ignore failure, can't really deal with it
}

void FdPollReader::run() {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    running_ = true;
  }
  running_cv_.notify_all();

  uint64_t stop_value = stopValue(); // reset the counter
  auto pollset = createPollSet(events_, stop_fd_);

  for (auto& event : events_) {
    event.enable();
  }

  bool run = true;

  while (run) {
    int ret = poll(
        pollset->pollfds->data(),
        pollset->pollfds->size(),
        -1 /*infinite timeout*/
    );

    if (ret == -1 && errno == EINTR) {
      // interrupted by a signal, keep going
      errno = 0;
      continue;
    }

    if (ret == 0) {
      throw std::logic_error("Infinite timeout but poll returned 0?");
    }

    // Invariant: ret > 0 i.e. at least one fd was signalled.
    // Walk the list to figure out which one.

    size_t stopfd_idx = pollset->pollfds->size() - 1;
    for (size_t i = 0; i < pollset->pollfds->size(); i++) {
      if (pollset->pollfds->at(i).revents == 0) {
        continue; // not signalled
      }

      if (i == stopfd_idx && stop_value != stopValue()) {
        run = false; // break outer loop
        continue;
      }

      // Invariant: Only the buffer fds are left at this point.
      Event* evt = pollset->events->at(i);
      if (evt == nullptr) {
        throw std::logic_error(
            "Invariant violation: reached buffer flush with no Event pointer");
      }
      detail::parser::parseBuffer(*evt, id_event_map_, listener_);
    }
  }

  for (auto& event : events_) {
    event.disable();
  }

  // Flush all buffers
  for (Event* event : *(pollset->events)) {
    if (event == nullptr) { // the entry for the stopfd is nullptr
      continue;
    }
    auto& evt = *event;
    detail::parser::parseBuffer(evt, id_event_map_, listener_);
  }

  if (listener_ != nullptr) {
    listener_->onReaderStop();
  }

  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    running_ = false;
  }
  running_cv_.notify_all();
}

void FdPollReader::stop() {
  uint64_t value = 1;
  // Signal the eventfd by writing to it
  ssize_t ret = write(stop_fd_, &value, sizeof(uint64_t));
  if (ret < 0) {
    throw std::system_error(errno, std::system_category());
  } else if (ret != sizeof(value)) {
    throw std::logic_error(
        "write() on eventfd wrote less than sizeof(uint64_t)");
  }

  std::unique_lock<std::mutex> lock(running_mutex_);
  running_cv_.wait(lock, [this] { return !this->running_; });
}

// Reads and resets the eventfd used to stop the reader.
uint64_t FdPollReader::stopValue() const {
  uint64_t value = 0;
  ssize_t ret = read(stop_fd_, &value, sizeof(value));
  if (ret < 0) {
    if (errno == EAGAIN) {
      errno = 0;
      return 0;
    } else {
      throw std::system_error(errno, std::system_category());
    }
  } else if (ret != sizeof(value)) {
    throw std::logic_error(
        "read() on eventfd returned non-uint64_t-sized value");
  }
  return value;
}

} // namespace detail
} // namespace yarn
} // namespace facebook
