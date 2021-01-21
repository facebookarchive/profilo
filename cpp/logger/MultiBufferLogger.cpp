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

#include "MultiBufferLogger.h"

namespace facebook {
namespace profilo {
namespace logger {

MultiBufferLogger::MultiBufferLogger(MultiBufferLogger::EntryIDCounter& counter)
    : entryID_(counter) {}

void MultiBufferLogger::addBuffer(std::shared_ptr<Buffer> buffer) {
  WriterLock lock(&lock_);
  buffers_.push_back(buffer);
}

void MultiBufferLogger::removeBuffer(std::shared_ptr<Buffer> buffer) {
  WriterLock lock(&lock_);
  auto iter = std::find(buffers_.begin(), buffers_.end(), buffer);
  if (iter == buffers_.end()) {
    return;
  }
  buffers_.erase(iter);
}

int32_t MultiBufferLogger::writeBytes(
    EntryType type,
    int32_t arg1,
    const uint8_t* arg2,
    size_t len) {
  if (len > Logger::kMaxVariableLengthEntry) {
    throw std::overflow_error("len is bigger than kMaxVariableLengthEntry");
  }
  if (arg2 == nullptr) {
    throw std::invalid_argument("arg2 is null");
  }

  // Maintain the same entry ID across all buffers
  auto id = entryID_.next();

  BytesEntry entry{
      .id = id,
      .type = type,
      .matchid = arg1,
      .bytes =
          {
              .values = arg2,
              .size = static_cast<uint16_t>(len),
          },
  };

  ReaderLock lock(&lock_);
  for (auto& buf : buffers_) {
    buf->logger().write(entry);
  }
  return entry.id;
}
} // namespace logger
} // namespace profilo
} // namespace facebook
