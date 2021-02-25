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
#include <atomic>

#include "PacketLogger.h"

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {

using namespace entries;

class Logger {
 public:
  struct EntryIDCounter {
    EntryIDCounter(int32_t initialValue) : id_(initialValue) {}
    EntryIDCounter(EntryIDCounter& copy) = delete;

    int32_t next() {
      int32_t value, newValue;

      do {
        value = id_.load();
        if (value <= 0 || value == std::numeric_limits<int32_t>::max()) {
          // explicitly handle overflow and skip negative IDs.
          newValue = 1;
        } else {
          newValue = value + 1;
        }
      } while (!id_.compare_exchange_weak(value, newValue));

      return value;
    }

   private:
    std::atomic<int32_t> id_;
  };

  static constexpr size_t kMaxVariableLengthEntry = 1024;
  // Start first entry shifted to allow safely adding extra entries to the trace
  // after completion.
  static constexpr int32_t kDefaultInitialID = 512;

  static EntryIDCounter& getGlobalEntryID();

  template <class T>
  int32_t write(T&& entry) {
    if (entry.id == 0) {
      entry.id = entryID_.next();
    }

    using U = std::decay_t<T>;
    auto size = U::calculateSize(entry);
    char payload[size];
    U::pack(entry, payload, size);

    logger_.write(payload, size);
    return entry.id;
  }

  template <class T>
  int32_t writeAndGetCursor(T&& entry, TraceBuffer::Cursor& cursor) {
    if (entry.id == 0) {
      entry.id = entryID_.next();
    }

    using U = std::decay_t<T>;
    auto size = U::calculateSize(entry);
    char payload[size];
    U::pack(entry, payload, size);

    cursor = logger_.writeAndGetCursor(payload, size);
    return entry.id;
  }

  PROFILOEXPORT int32_t
  writeBytes(EntryType type, int32_t arg1, const uint8_t* arg2, size_t len);

  // This constructor is for internal framework use.
  Logger(logger::TraceBufferProvider provider, EntryIDCounter& counter);

 private:
  EntryIDCounter& entryID_;
  logger::PacketLogger logger_;

  Logger(const Logger& other) = delete;
};

} // namespace profilo
} // namespace facebook
