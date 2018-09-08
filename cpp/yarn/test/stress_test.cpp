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

#include <stdlib.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include <yarn/Session.h>

using namespace facebook::yarn;

#ifndef ANDROID
static pid_t gettid() {
  return syscall(__NR_gettid);
}
#endif

const int NUM_THREADS = 10;
std::atomic<bool> run_perf(true);

void worker_thread() {
  int duration = std::rand() % 6 + 1;
  std::chrono::seconds wait_time(duration);

  uint64_t foo = 0xdeadbeef;
  auto now = std::chrono::steady_clock::now();
  auto endtime = now + wait_time;
  while (now < endtime) {
    // spin uselessly
    foo *= std::rand() * 0xfaceb00c;
    gettid();
    now = std::chrono::steady_clock::now();
  }
}

class PrintingListener : public RecordListener {
 public:
  virtual void onMmap(const RecordMmap& record) {
    std::cout << "mmap {"
              << "pid: " << record.pid << " tid: " << record.tid
              << " addr: " << record.addr << " len: " << record.len
              << " pgoff: " << record.pgoff << " filename: " << &record.filename
              << "}" << std::endl;
  }

  virtual void onSample(const EventType type, const RecordSample& record) {
    std::cout << "sample "
              << "(" << record.size() << ") "
              << "{"
              << "type: " << type << " pid: " << record.pid()
              << " tid: " << record.tid() << " cpu: "
              << record.cpu()
              //<< " ip: " << record.ip()
              << " id: " << record.id() << " (" << record.groupLeaderId() << ")"
              << " addr: " << record.addr() << " time: " << record.time()
              << " running: " << record.timeRunning()
              << " enabled: " << record.timeEnabled() << "}"
              << "\n";
  }

  virtual void onForkEnter(const RecordForkExit& record) {
    std::cout << "fork_enter {"
              << "pid: " << record.pid << " tid: " << record.tid
              << " ppid: " << record.ppid << " ptid: " << record.ptid
              << " time: " << record.time << "}" << std::endl;
  }

  virtual void onForkExit(const RecordForkExit& record) {
    std::cout << "fork_exit {"
              << "pid: " << record.pid << " tid: " << record.tid
              << " ppid: " << record.ppid << " ptid: " << record.ptid
              << " time: " << record.time << "}" << std::endl;
  }

  virtual void onLost(const RecordLost& record) {
    std::cout << "lost {"
              << "id: " << record.id << " lost: " << record.lost << "}"
              << std::endl;
  }

  virtual void onReaderStop() {
    std::cout << "onReaderStop()" << std::endl;
  }
};

void perf_thread() {
  std::cout << ">> Perf tid: " << gettid() << std::endl;
  EventSpec spec_clock = {.type = EVENT_TYPE_TASK_CLOCK,
                          .tid = EventSpec::kAllThreads};
  (void)spec_clock;
  EventSpec spec_ctx = {.type = EVENT_TYPE_CONTEXT_SWITCHES,
                        .tid = EventSpec::kAllThreads};
  EventSpec spec_cpu_clock = {.type = EVENT_TYPE_CPU_CLOCK,
                              .tid = EventSpec::kAllThreads};
  auto session = std::unique_ptr<Session>(new Session(
      {spec_ctx, spec_cpu_clock},
      {
          .fallbacks = FALLBACK_RAISE_RLIMIT,
          .maxAttachIterations = 3,
          .maxAttachedFdsRatio = 0.5,
      },
      std::unique_ptr<RecordListener>(new PrintingListener())));
  if (!session->attach()) {
    std::cout << ">> Could not attach!" << std::endl;
    return;
  }

  std::cout << ">> Starting read loop.." << std::endl;
  auto reader_thread = std::thread([&session]() {
    std::cout << ">> Reader tid: " << gettid() << std::endl;
    session->read();
  });

  while (run_perf) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << ">> Stopping reader.." << std::endl;
  session->stopRead();

  std::cout << ">> Waiting for thread to exit" << std::endl;
  reader_thread.join();
  std::cout << ">> Reader is stopped" << std::endl;
}

int main(int argc, char** argv) {
  std::cout << " >> Sleeping in case you want a debugger" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(7));
  std::cout << " >> Starting test" << std::endl;
  auto threads = std::vector<std::thread>();

  auto perf = std::thread(&perf_thread);

  std::cout << " >> Starting threads" << std::endl;
  for (int i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back(std::thread(&worker_thread));
  }

  for (auto& thread : threads) {
    thread.join();
  }
  run_perf.store(false, std::memory_order_seq_cst);
  perf.join();

  std::cout << " >> Ending test" << std::endl;
  return 0;
}
