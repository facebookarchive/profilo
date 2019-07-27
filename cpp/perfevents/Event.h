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

#include <system_error>
#include <vector>

#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_perf_event_open
#define __NR_perf_event_open 241
#endif

namespace facebook {
namespace perfevents {

class Event;
struct EventSpec;

using EventList = std::vector<Event>;
using EventSpecList = std::vector<EventSpec>;

// If you change this, you need to change the parser in SampleRecord
constexpr uint64_t kSampleType = PERF_SAMPLE_TID | PERF_SAMPLE_TIME |
    PERF_SAMPLE_ADDR | PERF_SAMPLE_ID | PERF_SAMPLE_STREAM_ID |
    PERF_SAMPLE_CPU | PERF_SAMPLE_READ;

// If you change this, you need to change the struct that Event::open() uses
constexpr uint64_t kReadFormat = PERF_FORMAT_TOTAL_TIME_ENABLED |
    PERF_FORMAT_TOTAL_TIME_RUNNING |
    PERF_FORMAT_ID; // needed to read the group leader id

enum EventType {
  EVENT_TYPE_NONE = 0,
  EVENT_TYPE_MAJOR_FAULTS = 1,
  EVENT_TYPE_MINOR_FAULTS = 2,
  EVENT_TYPE_CONTEXT_SWITCHES = 3,
  EVENT_TYPE_CPU_MIGRATIONS = 4,
  EVENT_TYPE_TASK_CLOCK = 5,
  EVENT_TYPE_CPU_CLOCK = 6,
};

// This is what users of this library use.
struct EventSpec {
  static const int32_t kAllThreads = 0xFFFFFFFF;

  EventType type;
  int32_t tid;

  inline bool isProcessWide() const {
    return tid == kAllThreads;
  }
};

class Event {
 public:
  explicit Event(EventType type, int32_t tid, int32_t cpu, bool inherit = true);
  Event();
  Event(Event const& evt) = delete;
  Event(Event&& evt);
  Event& operator=(Event const& evt) = delete;
  Event& operator=(Event&& evt);

  void open();
  uint64_t read() const;
  void close();

  void mmap(size_t sz);
  void munmap();

  void enable();
  void disable();

  void setOutput(const Event& evt);

  void* buffer() const;
  size_t bufferSize() const;
  int32_t cpu() const;
  int fd() const;
  int32_t tid() const;
  EventType type() const;
  perf_event_attr attr() const;

  // Returns the 64-bit in-kernel ID corresponding to this event. This is also
  // referenced under SAMPLE_ID in sampling records.
  uint64_t id() const;

  virtual ~Event();

 private:
  EventType type_;
  int32_t tid_;
  int32_t cpu_;
  int fd_;
  void* buffer_;
  size_t buffer_size_;

  uint64_t id_;

  perf_event_attr event_attr_;
};

} // namespace perfevents
} // namespace facebook
