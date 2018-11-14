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

#include <linux/perf_event.h>
#include <unistd.h>
#include <stdexcept>

#include <yarn/Event.h>

namespace facebook {
namespace yarn {

struct RecordForkExit {
  uint32_t pid, ppid;
  uint32_t tid, ptid;
  uint64_t time;
};

struct RecordMmap {
  uint32_t pid, tid;
  uint64_t addr;
  uint64_t len;
  uint64_t pgoff;
  char filename;
};

struct RecordLost {
  uint64_t id;
  uint64_t lost;
  // struct sample_id sample_id;
};

class RecordSample {
 public:
  // Memory management is left to the caller, this class
  // is just a facade and will perform no copies.
  RecordSample(
      void* data,
      size_t len,
      uint64_t sample_type,
      uint64_t read_format);

  // This object does not own any data and a copy may outlive
  // the pointed-to buffer.
  RecordSample(const RecordSample& other) = delete;

  uint64_t ip() const;
  uint32_t pid() const;
  uint32_t tid() const;
  uint64_t time() const;
  uint64_t addr() const;
  uint64_t groupLeaderId() const;
  uint64_t id() const;
  uint32_t cpu() const;
  uint64_t timeRunning() const;
  uint64_t timeEnabled() const;

  // Debugging:
  size_t size() const;

 private:
  uint8_t* data_;
  size_t len_;
  uint64_t sample_type_;
  uint64_t read_format_;

  size_t offsetForField(uint64_t field) const;
  size_t offsetForReadFormat(uint64_t field) const;
};

//
// Listener interface notified on every record read from the ring buffers.
// The objects received in the callbacks are guaranteed to exist only for the
// duration of the call.
//
struct RecordListener {
  virtual void onMmap(const RecordMmap& record) = 0;
  virtual void onSample(
      const EventType eventType,
      const RecordSample& record) = 0;
  virtual void onForkEnter(const RecordForkExit& record) = 0;
  virtual void onForkExit(const RecordForkExit& record) = 0;
  virtual void onLost(const RecordLost& record) = 0;
  virtual void onReaderStop() = 0;
  virtual ~RecordListener() = default;
};

} // namespace yarn
} // namespace facebook
