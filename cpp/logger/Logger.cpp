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

#include "Logger.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>

#include <profilo/util/common.h>

namespace facebook {
namespace profilo {

using namespace entries;

Logger::EntryIDCounter& Logger::getGlobalEntryID() {
  static EntryIDCounter global_instance{kDefaultInitialID};
  return global_instance;
}

Logger::Logger(logger::TraceBufferProvider provider, EntryIDCounter& counter)
    : entryID_(counter), logger_(provider) {}

int32_t Logger::writeBytes(
    EntryType type,
    int32_t arg1,
    const uint8_t* arg2,
    size_t len) {
  if (len > kMaxVariableLengthEntry) {
    throw std::overflow_error("len is bigger than kMaxVariableLengthEntry");
  }
  if (arg2 == nullptr) {
    throw std::invalid_argument("arg2 is null");
  }

  return write(BytesEntry{
      .id = 0,
      .type = type,
      .matchid = arg1,
      .bytes = {
          .values = const_cast<uint8_t*>(arg2),
          .size = static_cast<uint16_t>(len)}});
}

} // namespace profilo
} // namespace facebook
