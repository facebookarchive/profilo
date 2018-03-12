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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <utility>
#include <unistd.h>

#include <profilo/writer/TraceCallbacks.h>
#include <profilo/RingBuffer.h>
#include <profilo/LogEntry.h>
#include <profilo/entries/EntryParser.h>
#include <profilo/writer/PacketReassembler.h>

namespace facebook {
namespace profilo {
namespace writer {

class TraceWriter {
public:

  static const int64_t kStopLoopTraceID = 0;

  //
  // folder: the absolute path to the folder that will store any trace folders.
  // trace_prefix: a file prefix for every trace file written by this writer.
  // buffer: the ring buffer instance to use.
  // headers: a list of key-value headers to output at
  //          the beginning of the trace
  //
  TraceWriter(
    const std::string&& folder,
    const std::string&& trace_prefix,
    TraceBuffer& buffer,
    std::shared_ptr<TraceCallbacks> callbacks = nullptr,
    std::vector<std::pair<std::string, std::string>>&& headers =
      std::vector<std::pair<std::string, std::string>>()
  );

  //
  // Wait until a submit() call and then process a submitted trace ID.
  //
  void loop();

  //
  // Submit a trace ID for processing. Walk will start from `cursor`.
  // Will wake up the thread and let it run until the trace is finished.
  //
  // Call with trace_id = kStopLoopTraceID to terminate loop()
  // without processing a trace.
  //
  void submit(TraceBuffer::Cursor cursor, int64_t trace_id);

  //
  // Equivalent to write(buffer_.currentTail(), trace_id).
  // This will force the TraceWriter to scan the entire ring buffer for the
  // start event. Prefer the cursor version of submit() where appropriate.
  //
  void submit(int64_t trace_id);

private:
  std::mutex wakeup_mutex_;
  std::condition_variable wakeup_cv_;
  std::queue<std::pair<TraceBuffer::Cursor, int64_t>> wakeup_trace_ids_;

  const std::string trace_folder_;
  const std::string trace_prefix_;
  TraceBuffer& buffer_;
  std::vector<std::pair<std::string, std::string>> trace_headers_;

  std::shared_ptr<TraceCallbacks> callbacks_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
