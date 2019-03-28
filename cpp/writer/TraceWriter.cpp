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
#include <profilo/writer/MultiTraceLifecycleVisitor.h>
#include <profilo/writer/PacketReassembler.h>
#include <profilo/writer/TraceLifecycleVisitor.h>
#include <profilo/writer/TraceWriter.h>

#include <profilo/LogEntry.h>

namespace facebook {
namespace profilo {
namespace writer {

namespace {

void traceBackward(
    TraceLifecycleVisitor& visitor,
    TraceBuffer& buffer,
    TraceBuffer::Cursor& cursor) {
  PacketReassembler reassembler([&visitor](const void* data, size_t size) {
    EntryParser::parse(data, size, visitor);
  });

  TraceBuffer::Cursor backCursor{cursor};
  backCursor.moveBackward(); // Move back before trace start

  alignas(4) Packet packet;
  while (buffer.tryRead(packet, backCursor)) {
    reassembler.processBackwards(packet);

    if (!backCursor.moveBackward()) {
      break; // done
    }
  }
}
} // namespace

TraceWriter::TraceWriter(
    const std::string&& folder,
    const std::string&& trace_prefix,
    TraceBuffer& buffer,
    std::shared_ptr<TraceCallbacks> callbacks,
    std::vector<std::pair<std::string, std::string>>&& headers)
    : wakeup_mutex_(),
      wakeup_cv_(),
      wakeup_trace_ids_(),
      trace_folder_(std::move(folder)),
      trace_prefix_(std::move(trace_prefix)),
      buffer_(buffer),
      trace_headers_(std::move(headers)),
      callbacks_(callbacks) {}

void TraceWriter::loop() {
  while (true) {
    int64_t trace_id;
    // dummy call, no default constructor
    TraceBuffer::Cursor cursor = buffer_.currentTail();

    {
      std::unique_lock<std::mutex> lock(wakeup_mutex_);
      wakeup_cv_.wait(lock, [this, &trace_id, &cursor] {
        if (this->wakeup_trace_ids_.empty()) {
          return false;
        }
        std::tie(cursor, trace_id) = this->wakeup_trace_ids_.front();
        this->wakeup_trace_ids_.pop();
        return true;
      });
    }

    // Magic signal to terminate the loop.
    if (trace_id == kStopLoopTraceID) {
      return;
    }

    {
      MultiTraceLifecycleVisitor visitor(
          trace_folder_,
          trace_prefix_,
          callbacks_,
          trace_headers_,
          [this, &cursor](TraceLifecycleVisitor& visitor) {
            traceBackward(visitor, buffer_, cursor);
          });

      PacketReassembler reassembler([&visitor](const void* data, size_t size) {
        EntryParser::parse(data, size, visitor);
      });

      while (!visitor.done()) {
        alignas(4) Packet packet;
        if (!buffer_.waitAndTryRead(packet, cursor)) {
          // Missed event, abort.
          visitor.abort(AbortReason::MISSED_EVENT);
          break;
        }
        reassembler.process(packet);
        cursor.moveForward();
      }

      { // Cleanup of processed traces from the wakeup queue
        std::lock_guard<std::mutex> lock(wakeup_mutex_);
        auto consumed_traces = visitor.getConsumedTraces();
        while (!wakeup_trace_ids_.empty()) {
          auto& item = wakeup_trace_ids_.front();
          if (consumed_traces.find(item.second) == consumed_traces.end()) {
            break;
          }
          wakeup_trace_ids_.pop();
        }
      }
    }
  }
}

void TraceWriter::submit(TraceBuffer::Cursor cursor, int64_t trace_id) {
  {
    std::lock_guard<std::mutex> lock(wakeup_mutex_);
    wakeup_trace_ids_.push(std::make_pair(cursor, trace_id));
  }
  wakeup_cv_.notify_all();
}

void TraceWriter::submit(int64_t trace_id) {
  submit(buffer_.currentTail(), trace_id);
}

} // namespace writer
} // namespace profilo
} // namespace facebook
