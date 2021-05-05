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

#include <profilo/logger/BlockLogger.h>
#include <util/common.h>

namespace facebook {
namespace profilo {
namespace logger {

BlockLogger::BlockLogger(MultiBufferLogger& logger, const char* name)
    : logger_(logger), tid_(threadID()) {
  auto id = logger_.write(StandardEntry{
      .type = EntryType::MARK_PUSH,
      .timestamp = monotonicTime(),
      .tid = tid_,
  });

  logger_.writeBytes(
      EntryType::STRING_NAME,
      id,
      reinterpret_cast<const uint8_t*>(name),
      strlen(name));
}
BlockLogger::~BlockLogger() {
  logger_.write(StandardEntry{
      .type = EntryType::MARK_POP,
      .timestamp = monotonicTime(),
      .tid = tid_,
  });
}

} // namespace logger
} // namespace profilo
} // namespace facebook
