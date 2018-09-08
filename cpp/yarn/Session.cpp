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

#include <yarn/Session.h>

namespace facebook {
namespace yarn {

Session::Session(
    const std::vector<EventSpec>& events,
    const SessionSpec& spec,
    std::unique_ptr<RecordListener> listener)
    : events_(events),
      spec_(spec),
      perf_events_(),
      reader_(nullptr),
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
    reader_ = nullptr;

    return true;
  } catch (std::system_error& ex) {
    return false;
  }
}

void Session::detach() {
  reader_ = nullptr;
  perf_events_ = EventList();
}

void Session::read() {
  if (perf_events_.empty()) {
    throw std::logic_error("Cannot create reader for unattached Session");
  }

  if (reader_ == nullptr) {
    reader_ = detail::make_unique<detail::FdPollReader>(
        perf_events_, listener_.get());
  }
  reader_->run();
}

void Session::stopRead() {
  if (reader_ == nullptr) {
    return;
  }
  reader_->stop();
}

} // namespace yarn
} // namespace facebook
