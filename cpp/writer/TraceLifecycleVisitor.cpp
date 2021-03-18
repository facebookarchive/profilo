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

#include <errno.h>
#include <stdio.h>

#include <system_error>

#include <profilo/writer/DeltaEncodingVisitor.h>
#include <profilo/writer/PrintEntryVisitor.h>
#include <profilo/writer/StackTraceInvertingVisitor.h>
#include <profilo/writer/TimestampTruncatingVisitor.h>
#include <profilo/writer/TraceLifecycleVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

using namespace facebook::profilo::entries;

TraceLifecycleVisitor::TraceLifecycleVisitor(
    const std::string& trace_folder,
    const std::string& trace_prefix,
    std::shared_ptr<TraceCallbacks> callbacks,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int64_t trace_id,
    std::function<void(TraceLifecycleVisitor& visitor)> trace_backward_callback)
    :

      trace_folder_(trace_folder),
      trace_prefix_(trace_prefix),
      trace_headers_(headers),
      output_(nullptr),
      delegates_(),
      expected_trace_(trace_id),
      callbacks_(callbacks),
      started_(false),
      done_(false),
      trace_backward_callback_(std::move(trace_backward_callback)) {}

void TraceLifecycleVisitor::visit(const StandardEntry& entry) {
  auto type = static_cast<EntryType>(entry.type);
  switch (type) {
    case EntryType::TRACE_END: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceEnd(trace_id);
      break;
    }
    case EntryType::TRACE_TIMEOUT:
    case EntryType::TRACE_ABORT: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      auto reason = type == EntryType::TRACE_TIMEOUT
          ? AbortReason::TIMEOUT
          : AbortReason::CONTROLLER_INITIATED;

      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceAbort(trace_id, reason);
      break;
    }
    case EntryType::TRACE_BACKWARDS:
    case EntryType::TRACE_START: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      onTraceStart(trace_id, entry.matchid);
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }

      if (type == EntryType::TRACE_BACKWARDS) {
        trace_backward_callback_(*this);
      }
      break;
    }
    case EntryType::LOGGER_PRIORITY: {
      if (expected_trace_ == entry.extra) {
        thread_priority_ = std::make_unique<ScopedThreadPriority>(entry.callid);
      }
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      break;
    }
    default: {
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
    }
  }
}

void TraceLifecycleVisitor::visit(const FramesEntry& entry) {
  if (hasDelegate()) {
    delegates_.back()->visit(entry);
  }
}

void TraceLifecycleVisitor::visit(const BytesEntry& entry) {
  if (hasDelegate()) {
    delegates_.back()->visit(entry);
  }
}

void TraceLifecycleVisitor::abort(AbortReason reason) {
  onTraceAbort(expected_trace_, reason);
}

void TraceLifecycleVisitor::onTraceStart(int64_t trace_id, int32_t flags) {
  if (output_ != nullptr) {
    // active trace with same ID, abort
    abort(AbortReason::NEW_START);
    return;
  }

  output_ = TraceFileHelpers::openCompressedStream(
      trace_id, trace_folder_, trace_prefix_);
  TraceFileHelpers::writeHeaders(*output_, trace_id, trace_headers_);

  // outputTime = truncate(current) - truncate(prev)
  delegates_.emplace_back(new PrintEntryVisitor(*output_));
  delegates_.emplace_back(new DeltaEncodingVisitor(*delegates_.back()));
  delegates_.emplace_back(new TimestampTruncatingVisitor(
      *delegates_.back(), TraceFileHelpers::kTimestampPrecision));
  delegates_.emplace_back(new StackTraceInvertingVisitor(*delegates_.back()));

  if (callbacks_.get() != nullptr) {
    callbacks_->onTraceStart(trace_id, flags);
  }

  started_ = true;
  done_ = false;
}

void TraceLifecycleVisitor::onTraceAbort(int64_t trace_id, AbortReason reason) {
  done_ = true;
  cleanupState();
  if (started_ && callbacks_.get() != nullptr) {
    callbacks_->onTraceAbort(trace_id, reason);
  }
}

void TraceLifecycleVisitor::onTraceEnd(int64_t trace_id) {
  done_ = true;
  cleanupState();
  if (started_ && callbacks_.get() != nullptr) {
    callbacks_->onTraceEnd(trace_id);
  }
}

void TraceLifecycleVisitor::cleanupState() {
  delegates_.clear();
  thread_priority_ = nullptr;
  if (output_) {
    output_->flush();
    output_->close();
    output_ = nullptr;
  }
}

} // namespace writer
} // namespace profilo
} // namespace facebook
