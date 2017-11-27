// Copyright 2004-present Facebook. All Rights Reserved.

#include <yarn/Session.h>


namespace facebook {
namespace yarn {

Session::Session(
  const std::vector<EventSpec>& events,
  const SessionSpec& spec,
  std::unique_ptr<RecordListener> listener
):events_(events),
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
    reader_ = detail::make_unique<detail::FdPollReader>(perf_events_, listener_.get());
  }
  reader_->run();
}

void Session::stopRead() {
  if (reader_ == nullptr) {
    return;
  }
  reader_->stop();
}


} // yarn
} // facebook
