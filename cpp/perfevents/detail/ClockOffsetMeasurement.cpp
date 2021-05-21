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

#include <profilo/perfevents/Session.h>
#include <profilo/perfevents/detail/ClockOffsetMeasurement.h>

#include <stdint.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace facebook {
namespace perfevents {
namespace detail {
namespace clock {

namespace {

struct ClockOffsetListener : public RecordListener {
  ClockOffsetListener(std::atomic<int64_t>& faultTime)
      : fault_time_(faultTime) {
    fault_time_ = -1;
  }

  virtual void onMmap(const RecordMmap& record) {}
  virtual void onSample(const EventType eventType, const RecordSample& record) {
    if (eventType == EVENT_TYPE_MINOR_FAULTS) {
      fault_time_ = record.time();
    }
  }
  virtual void onForkEnter(const RecordForkExit& record) {}
  virtual void onForkExit(const RecordForkExit& record) {}
  virtual void onLost(const RecordLost& record) {}
  virtual void onReaderStop() {}

  std::atomic<int64_t>& fault_time_;
};
} // namespace

int64_t measureOffsetFromPerfClock(clockid_t clockid) {
  // The idea here is to
  // 1) start a session looking for minor faults from a target thread *only*
  // 2) capture the clockid_t timestamp before
  // 3) incur a minor fault via mmap(3)
  // 4) capture the clockid_t timestamp after
  // 5) average the clockid_t timestamps and compute an offset from the value
  // that the session sees.
  //
  // This requires coordinating multiple threads:
  // a) the caller thread is orchestrating the whole thing
  // b) a measurement thread will incur the actual minor fault
  // c) a session thread will actually read the perf events for the measurement
  // thread

  std::atomic<int32_t> threadID{};
  std::atomic<int64_t> faultClockTime{};
  std::atomic<int64_t> faultKernelTime{};

  // Coordinating condition variables, flags, and mutex.
  std::mutex cond_mutex;
  bool measurement_thread_has_started = false, session_has_started = false;
  std::condition_variable measurement_thread_has_started_cond;
  std::condition_variable session_has_started_cond;

  std::thread measurementThread([&] {
    threadID = syscall(__NR_gettid);
    faultClockTime = -1;

    // We've populated the thread ID, notify the main thread.
    {
      std::lock_guard<std::mutex> lg(cond_mutex);
      measurement_thread_has_started = true;
    }
    measurement_thread_has_started_cond.notify_all();

    // Wait until the perf events session has been started
    {
      std::unique_lock<std::mutex> lock(cond_mutex);
      session_has_started_cond.wait(lock, [&] { return session_has_started; });
    }

    // We need new address space in order to incur an actual fault. malloc() may
    // reuse memory.
    void* area = mmap(
        nullptr,
        PAGE_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        /*fd*/ -1,
        /*offset*/ 0);
    if (area == MAP_FAILED) {
      return;
    }

    struct timespec before {
    }, after{};
    if (clock_gettime(clockid, &before)) {
      return;
    }

    // incur actual fault
    *reinterpret_cast<uint32_t*>(area) = 0xfaceb00c;

    if (clock_gettime(clockid, &after)) {
      return;
    }

    if (munmap(area, PAGE_SIZE)) {
      return;
    }

    int64_t beforeTs =
        static_cast<int64_t>(before.tv_sec) * 1000000000 + before.tv_nsec;
    int64_t afterTs =
        static_cast<int64_t>(after.tv_sec) * 1000000000 + after.tv_nsec;

    faultClockTime = beforeTs + (afterTs - beforeTs) / 2;
  });

  // Wait for the measurement thread to start so we can read the thread id.
  {
    std::unique_lock<std::mutex> lock(cond_mutex);
    measurement_thread_has_started_cond.wait(
        lock, [&] { return measurement_thread_has_started; });
  }

  std::vector<EventSpec> eventSpecs(1);
  eventSpecs[0] = EventSpec{
      .type = EVENT_TYPE_MINOR_FAULTS,
      .tid = threadID.load(),
  };

  SessionSpec sessionSpec{
      .fallbacks = 0,
      .maxAttachIterations = 1,
      .maxAttachedFdsRatio = 1.0f,
  };

  Session session(
      eventSpecs,
      sessionSpec,
      std::make_unique<ClockOffsetListener>(faultKernelTime));
  if (!session.attach()) {
    //
    // Let the measurement thread run and wait for it to finish before
    // exiting with an error.
    //
    {
      std::lock_guard<std::mutex> lg(cond_mutex);
      session_has_started = true;
    }
    session_has_started_cond.notify_all();

    measurementThread.join();
    return INT64_MIN;
  }

  std::thread sessionThread([&] {
    {
      std::lock_guard<std::mutex> lg(cond_mutex);
      session_has_started = true;
    }
    session_has_started_cond.notify_all();

    session.run();
  });

  {
    std::unique_lock<std::mutex> lock(cond_mutex);
    session_has_started_cond.wait(lock, [&] { return session_has_started; });
  }

  measurementThread.join();
  session.stop();
  session.detach();
  sessionThread.join();

  if (faultClockTime.load() == -1 || faultKernelTime.load() == -1) {
    return INT64_MIN;
  }

  return faultClockTime.load() - faultKernelTime.load();
}

} // namespace clock
} // namespace detail
} // namespace perfevents
} // namespace facebook
