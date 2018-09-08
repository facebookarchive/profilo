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

#include <yarn/Event.h>

namespace facebook {
namespace yarn {

static perf_event_attr
createEventAttr(EventType type, int32_t tid, int32_t cpu, bool inherit) {
  perf_event_attr attr{};
  attr.size = sizeof(struct perf_event_attr);

  attr.wakeup_events = 1; // TODO tune this value
  attr.watermark = 0; // 0 == count in wakeup_events, 1 == count in
                      // wakeup_watermark (in bytes)

  switch (type) {
    case EventType::EVENT_TYPE_MAJOR_FAULTS: {
      attr.type = PERF_TYPE_SOFTWARE;
      attr.config = PERF_COUNT_SW_PAGE_FAULTS_MAJ;
      attr.sample_period = 1;
      break;
    }
    case EventType::EVENT_TYPE_CONTEXT_SWITCHES: {
      attr.type = PERF_TYPE_SOFTWARE;
      attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
      attr.sample_period = 1;
      break;
    }
    case EventType::EVENT_TYPE_CPU_MIGRATIONS: {
      attr.type = PERF_TYPE_SOFTWARE;
      attr.config = PERF_COUNT_SW_CPU_MIGRATIONS;
      attr.sample_period = 1;
      break;
    }
    case EventType::EVENT_TYPE_TASK_CLOCK: {
      attr.type = PERF_TYPE_SOFTWARE;
      attr.config = PERF_COUNT_SW_TASK_CLOCK;
      attr.sample_freq = 1000; // 1000 Hz == 1ms
      attr.freq = 1;
      break;
    }
    case EventType::EVENT_TYPE_CPU_CLOCK: {
      attr.type = PERF_TYPE_SOFTWARE;
      attr.config = PERF_COUNT_SW_CPU_CLOCK;
      attr.sample_freq = 1000;
      attr.freq = 1;
      break;
    }
    default:
      throw std::invalid_argument("Unknown event type");
  }

  attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ADDR |
      PERF_SAMPLE_ID | PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_CPU |
      PERF_SAMPLE_READ;

  // If you change this, update the struct in open() as well
  attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
      PERF_FORMAT_TOTAL_TIME_RUNNING |
      PERF_FORMAT_ID; // needed to read the group leader id

  attr.disabled = 1;

  if (inherit) {
    attr.inherit = 1;
  }
  return attr;
}

static int perf_event_open(
    perf_event_attr* attr,
    pid_t pid,
    int cpu,
    int group_fd,
    unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

Event::Event(EventType type, int32_t tid, int32_t cpu, bool inherit)
    : type_(type),
      tid_(tid),
      cpu_(cpu),
      fd_(-1),
      buffer_(nullptr),
      buffer_size_(0),
      id_(0),
      event_attr_(createEventAttr(type, tid, cpu, inherit)) {}

Event::Event()
    : type_(EVENT_TYPE_NONE),
      tid_(-1),
      cpu_(-1),
      fd_(-1),
      buffer_(nullptr),
      buffer_size_(0),
      id_(0),
      event_attr_() {}

Event::Event(Event&& other)
    : type_(std::move(other.type_)),
      tid_(std::move(other.tid_)),
      cpu_(std::move(other.cpu_)),
      fd_(std::move(other.fd_)),
      buffer_(std::move(other.buffer_)),
      buffer_size_(std::move(other.buffer_size_)),
      id_(std::move(other.id_)),
      event_attr_(std::move(other.event_attr_)) {
  other.fd_ = -1;
  other.buffer_ = nullptr;
  other.buffer_size_ = 0;
  other.id_ = 0;
}

Event::~Event() {
  if (buffer_ != nullptr) {
    try {
      munmap();
    } catch (std::system_error& ex) {
      // Intentionally ignored
    }
  }
  if (fd_ != -1) {
    try {
      disable();
    } catch (std::system_error& ex) {
      // Intentionally ignored
    }
    // Regardless of whether disable() threw an exception,
    // still try to close the fd.
    try {
      close();
    } catch (std::system_error& ex) {
      // Intentionally ignored
    }
  }
}

struct read_format {
  uint64_t value;
  uint64_t time_enabled;
  uint64_t time_remaining;
  uint64_t id;
};

static read_format readFromFd(int fd, const perf_event_attr& attr) {
  const int32_t expected_read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
      PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;

  if ((attr.read_format & expected_read_format) != expected_read_format) {
    throw std::logic_error("read_format does not have expected struct fields");
  }

  read_format data{};
  if (::read(fd, &data, sizeof(data)) != sizeof(data)) {
    throw std::system_error(errno, std::system_category());
  }
  return data;
}

void Event::open() {
  int fd =
      perf_event_open(&event_attr_, tid_, cpu_, /*group_fd*/ -1, /*flags*/ 0);
  if (fd == -1) {
    throw std::system_error(errno, std::system_category());
  }
  fd_ = fd;

  try {
    read_format data = readFromFd(fd, event_attr_);
    id_ = data.id;
  } catch (std::system_error& ex) {
    // Clean up the open fd, we don't want to deal with events without an ID
    close();
    throw;
  }
}

uint64_t Event::read() const {
  if (fd_ == -1) {
    throw std::invalid_argument("Cannot close an unopened event");
  }
  read_format data = readFromFd(fd_, event_attr_);
  return data.value;
}

void Event::close() {
  if (fd_ == -1) {
    throw std::invalid_argument("Cannot close an unopened event");
  }
  if (::close(fd_) == -1) {
    throw std::system_error(errno, std::system_category());
  }
  fd_ = -1;
}

void Event::mmap(size_t sz) {
  if (fd_ == -1) {
    throw std::invalid_argument("Cannot mmap an unopened event");
  }

  void* buffer = ::mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (buffer == MAP_FAILED) {
    throw std::system_error(errno, std::system_category());
  }
  buffer_ = buffer;
  buffer_size_ = sz;
}

void Event::munmap() {
  if (buffer_ == nullptr) {
    throw std::invalid_argument("Cannot munmap an unmmap'd event");
  }
  if (::munmap(buffer_, buffer_size_) == -1) {
    throw std::system_error(errno, std::system_category());
  }
  buffer_ = nullptr;
  buffer_size_ = 0;
}

void Event::enable() {
  if (fd_ == -1) {
    throw std::invalid_argument("Cannot enable an unopened event");
  }
  if (ioctl(fd_, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1) {
    throw std::system_error(errno, std::system_category());
  }
}

void Event::disable() {
  if (fd_ == -1) {
    throw std::invalid_argument("Cannot disable an unopened event");
  }
  if (ioctl(fd_, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) == -1) {
    throw std::system_error(errno, std::system_category());
  }
}

void Event::setOutput(const Event& event) {
  // The kernel returns EINVAL for all of these, differentiate them explicitly
  // for easier diagnostics.
  if (fd_ == -1) {
    throw std::invalid_argument("Cannot set output on an unopened event");
  }
  if (event.fd_ == -1) {
    throw std::invalid_argument("Cannot set output to unopened event");
  }
  if (cpu_ != event.cpu_) {
    throw std::invalid_argument("Output target must be on the same CPU");
  }
  if (event.buffer() == nullptr) {
    throw std::invalid_argument("Output must be mapped already");
  }
  if (event.attr().sample_type != attr().sample_type) {
    // We need all events in a single ring buffer to use the same sample_type.
    // If they don't, the ring buffer data is unparseable on older
    // kernels. Linux added PERF_SAMPLE_IDENTIFIER in 3.12 to address this
    // issue but we can't rely on that on all the devices we want to support.
    //
    // c.f. the section on PERF_SAMPLE_IDENTIFIER in perf_event_open(2)
    throw std::invalid_argument(
        "Parent and child must agree on perf_event_attr.sample_type");
  }
  if (ioctl(fd_, PERF_EVENT_IOC_SET_OUTPUT, event.fd_) == -1) {
    throw std::system_error(errno, std::system_category());
  }
}

int Event::fd() const {
  return fd_;
}

void* Event::buffer() const {
  return buffer_;
}

size_t Event::bufferSize() const {
  return buffer_size_;
}

int32_t Event::tid() const {
  return tid_;
}

int32_t Event::cpu() const {
  return cpu_;
}

perf_event_attr Event::attr() const {
  return event_attr_;
}

uint64_t Event::id() const {
  return id_;
}

EventType Event::type() const {
  return type_;
}

} // namespace yarn
} // namespace facebook
