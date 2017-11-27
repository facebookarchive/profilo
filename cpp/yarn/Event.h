// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <system_error>
#include <vector>

#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_perf_event_open
# define __NR_perf_event_open 241
#endif

namespace facebook {
namespace yarn {

class Event;
struct EventSpec;

using EventList = std::vector<Event>;
using EventSpecList = std::vector<EventSpec>;

enum EventType {
  EVENT_TYPE_NONE = 0,
  EVENT_TYPE_MAJOR_FAULTS = 1,
  EVENT_TYPE_CONTEXT_SWITCHES = 2,
  EVENT_TYPE_CPU_MIGRATIONS = 3,
  EVENT_TYPE_TASK_CLOCK = 4,
  EVENT_TYPE_CPU_CLOCK = 5,
};

// This is what users of this library use.
struct EventSpec {
  static const int32_t kAllThreads = 0xFFFFFFFF;

  const EventType type;
  const int32_t tid;

  inline bool isProcessWide() const {
    return tid == kAllThreads;
  }
};

class Event {

public:
  explicit Event(EventType type, int32_t tid, int32_t cpu, bool inherit = true);
  Event();
  Event(Event& evt) = delete;
  Event(Event&& evt);
  Event& operator=(Event& evt) = delete;
  Event& operator=(Event&& evt) = delete;

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

} // yarn
} // facebook
