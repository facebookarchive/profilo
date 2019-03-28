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

#include <profilo/LogEntry.h>
#include <profilo/entries/Entry.h>
#include <profilo/entries/EntryType.h>

#include "PacketLogger.h"
#include "RingBuffer.h"

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {

using namespace entries;

class Logger {
  const int32_t TRACING_DISABLED = -1;
  const int32_t NO_MATCH = 0;

 public:
  const size_t kMaxVariableLengthEntry = 1024;

  PROFILOEXPORT static Logger& get();

  template <class T>
  int32_t write(T&& entry, uint16_t id_step = 1) {
    entry.id = nextID(id_step);

    auto size = T::calculateSize(entry);
    char payload[size];
    T::pack(entry, payload, size);

    logger_.write(payload, size);
    return entry.id;
  }

  template <class T>
  int32_t writeAndGetCursor(T&& entry, TraceBuffer::Cursor& cursor) {
    entry.id = nextID();

    auto size = T::calculateSize(entry);
    char payload[size];
    T::pack(entry, payload, size);

    cursor = logger_.writeAndGetCursor(payload, size);
    return entry.id;
  }

  PROFILOEXPORT int32_t
  writeBytes(EntryType type, int32_t arg1, const uint8_t* arg2, size_t len);

  PROFILOEXPORT void writeStackFrames(
      int32_t tid,
      int64_t time,
      const int64_t* methods,
      uint8_t depth,
      EntryType entry_type = entries::STACK_FRAME);

  PROFILOEXPORT void writeTraceAnnotation(int32_t key, int64_t value);

 private:
  std::atomic<int32_t> entryID_;
  logger::PacketLogger logger_;

  Logger(logger::PacketBufferProvider provider)
      : entryID_(0), logger_(provider) {}
  Logger(const Logger& other) = delete;

  inline int32_t nextID(uint16_t step = 1) {
    int32_t id;
    do {
      id = entryID_.fetch_add(step);
    } while (id == TRACING_DISABLED || id == NO_MATCH);
    return id;
  }
};

} // namespace profilo
} // namespace facebook
