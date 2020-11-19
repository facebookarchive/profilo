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

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#include <profilo/entries/EntryParser.h>
#include <profilo/writer/DeltaEncodingVisitor.h>
#include <profilo/writer/PacketReassembler.h>
#include <profilo/writer/TraceLifecycleVisitor.h>
#include <profilo/writer/TraceWriter.h>

#include <profilo/LogEntry.h>

namespace facebook {
namespace profilo {
namespace writer {

TraceWriter::TraceWriter(
    const std::string&& folder,
    const std::string&& trace_prefix,
    std::shared_ptr<Buffer> buffer,
    std::shared_ptr<TraceCallbacks> callbacks,
    std::vector<std::pair<std::string, std::string>>&& headers,
    TraceBackwardsCallback trace_backwards_callback)
    : wakeup_mutex_(),
      wakeup_cv_(),
      wakeup_trace_id_(nullptr),
      stop_requested_(false),
      trace_folder_(std::move(folder)),
      trace_prefix_(std::move(trace_prefix)),
      buffer_(std::move(buffer)),
      trace_headers_(std::move(headers)),
      callbacks_(callbacks),
      trace_backwards_callback_(trace_backwards_callback) {}

int64_t TraceWriter::processTrace(
    int64_t trace_id,
    TraceBuffer::Cursor& cursor) {
  TraceLifecycleVisitor visitor(
      trace_folder_,
      trace_prefix_,
      callbacks_,
      trace_headers_,
      trace_id,
      [this, &cursor](TraceLifecycleVisitor& visitor) {
        if (trace_backwards_callback_ == nullptr) {
          return;
        }
        trace_backwards_callback_(visitor, buffer_->ringBuffer(), cursor);
      });

  PacketReassembler reassembler([&visitor](const void* data, size_t size) {
    EntryParser::parse(data, size, visitor);
  });

  while (!visitor.done()) {
    alignas(4) Packet packet;
    if (!buffer_->ringBuffer().waitAndTryRead(packet, cursor)) {
      // Missed event, abort.
      visitor.abort(AbortReason::MISSED_EVENT);
      break;
    }
    reassembler.process(packet);
    cursor.moveForward();
  }

  return visitor.getTraceID();
}

void TraceWriter::loop() {
  int64_t trace_id = 0;
  // dummy call, no default constructor
  TraceBuffer::Cursor cursor = buffer_->ringBuffer().currentTail();
  bool stop_requested = false;

  {
    std::unique_lock<std::mutex> lock(wakeup_mutex_);
    wakeup_cv_.wait(lock, [this, &trace_id, &cursor, &stop_requested] {
      stop_requested = stop_requested_;
      bool woken_up = false;
      if (wakeup_trace_id_) {
        std::tie(cursor, trace_id) = *wakeup_trace_id_;
        wakeup_trace_id_ = nullptr;
        woken_up = true;
      }
      return stop_requested || woken_up;
    });
  }

  if (trace_id != 0) {
    processTrace(trace_id, cursor);
  }
}

void TraceWriter::submit(TraceBuffer::Cursor cursor, int64_t trace_id) {
  {
    std::lock_guard<std::mutex> lock(wakeup_mutex_);
    if (trace_id == kStopLoopTraceID) {
      stop_requested_ = true;
      wakeup_trace_id_ = nullptr;
    } else {
      stop_requested_ = false;
      wakeup_trace_id_ =
          std::make_unique<std::pair<TraceBuffer::Cursor, int64_t>>(
              std::make_pair(cursor, trace_id));
    }
  }
  wakeup_cv_.notify_all();
}

void TraceWriter::submit(int64_t trace_id) {
  submit(buffer_->ringBuffer().currentTail(), trace_id);
}

} // namespace writer
} // namespace profilo
} // namespace facebook
