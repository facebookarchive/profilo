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

#include <fb/log.h>
#include <profilo/perfevents/Session.h>
#include <profilo/perfevents/detail/AttachmentStrategy.h>
#include <profilo/perfevents/detail/make_unique.h>

namespace facebook {
namespace perfevents {

Session::Session(
    const std::vector<EventSpec>& events,
    const SessionSpec& spec,
    std::unique_ptr<RecordListener> listener)
    : events_(events),
      spec_(spec),
      reader_(nullptr),
      perf_events_(),
      listener_(std::move(listener)) {}

bool Session::attach() {
  if (!perf_events_.empty()) {
    throw std::runtime_error("Session already attached");
  }

  auto strategy = detail::PerCoreAttachmentStrategy(
      events_,
      spec_.fallbacks,
      spec_.maxAttachIterations,
      spec_.maxAttachedFdsRatio);

  try {
    auto events = strategy.attach();
    if (events.empty()) {
      return false;
    }

    perf_events_ = std::move(events);

    for (auto& evt : perf_events_) {
      evt.enable();
    }
    {
      std::lock_guard<std::mutex> lg(reader_mtx_);
      reader_ = detail::make_unique<detail::FdPollReader>(
          perf_events_, listener_.get());
    }

    return true;
  } catch (std::system_error& ex) {
    FBLOGW("Session failed to attach: %s", ex.what());
    return false;
  }
}

void Session::detach() {
  {
    std::lock_guard<std::mutex> lg(reader_mtx_);
    reader_ = nullptr;
  }
  for (auto& evt : perf_events_) {
    evt.disable();
  }
  perf_events_ = EventList();
}

void Session::run() {
  detail::Reader* reader;
  {
    // Read under the lock to ensure we synchronize with the write in attach()
    std::lock_guard<std::mutex> lg(reader_mtx_);
    reader = reader_.get();
  }
  if (!reader) {
    throw std::logic_error("Calling read() on an unattached session!");
  }
  reader->run();
}

void Session::stop() {
  detail::Reader* reader;
  {
    std::lock_guard<std::mutex> lg(reader_mtx_);
    reader = reader_.get();
  }
  if (reader == nullptr) {
    throw std::logic_error("No reader, did you call attach()?");
  }
  reader->stop();
}

} // namespace perfevents
} // namespace facebook
