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

#include <pthread.h>
#include <functional>
#include <memory>
#include <vector>

#include <linker/locks.h>
#include <profilo/mmapbuf/Buffer.h>

using namespace facebook::profilo::mmapbuf;

namespace facebook {
namespace profilo {
namespace logger {

class MultiBufferLogger {
 public:
  using EntryIDCounter = Logger::EntryIDCounter;
  explicit MultiBufferLogger(
      EntryIDCounter& counter = Logger::getGlobalEntryID());

  void addBuffer(std::shared_ptr<Buffer> buffer);
  void removeBuffer(std::shared_ptr<Buffer> buffer);

  template <class T>
  int32_t write(T&& entry) {
    // Maintain the same ID across all buffers
    auto id = entryID_.next();
    entry.id = id;

    ReaderLock lock(&lock_);
    for (auto& buf : buffers_) {
      buf->logger().write(entry);
    }
    return entry.id;
  }

  int32_t
  writeBytes(EntryType type, int32_t arg1, const uint8_t* arg2, size_t len);

 private:
  pthread_rwlock_t lock_ = PTHREAD_RWLOCK_INITIALIZER;
  std::vector<std::shared_ptr<Buffer>> buffers_{};
  EntryIDCounter& entryID_;
};

} // namespace logger
} // namespace profilo
} // namespace facebook
