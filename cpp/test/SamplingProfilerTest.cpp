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

#include <memory>

#include <gtest/gtest.h>

#include <fb/log.h>
#include <signal.h>
#include <array>
#include <atomic>
#include <cinttypes>
#include <memory>
#include <thread>

#include <phaser.h>
#include <profilo/LogEntry.h>
#include <profilo/profiler/SamplingProfiler.h>
#include <profilo/profiler/SignalHandler.h>
#include <profilo/test/TestSequencer.h>
#include <profilo/util/common.h>
#include "../logger/MultiBufferLogger.h"

namespace facebook {
namespace profilo {
namespace profiler {
constexpr auto kNanosecondsInMicrosecond = 1000;
constexpr auto kMicrosecondsInSecond = 1000 * 1000;
constexpr auto kMicrosecondsInMillisecond = 1000;
constexpr auto kHalfHourInMilliseconds = 1800 * 1000;

constexpr auto kDefaultSampleIntervalMs = kHalfHourInMilliseconds;
constexpr auto kDefaultThreadDetectIntervalMs = kHalfHourInMilliseconds;
constexpr bool kDefaultUseWallClockSetting = false;

/* Scopes all access to private data from the SamplingProfiler instance*/
class SamplingProfilerTestAccessor {
 public:
  explicit SamplingProfilerTestAccessor(SamplingProfiler& profiler)
      : profiler_(profiler) {}

  bool isProfiling() const {
    return profiler_.state_.isProfiling.load();
  }

  bool isLoggerLoopDone() const {
    return profiler_.state_.isLoggerLoopDone.load();
  }

  int countSlotsWithPredicate(std::function<bool(StackSlot const&)> pred) {
    return std::count_if(
        std::begin(profiler_.state_.stacks),
        std::end(profiler_.state_.stacks),
        pred);
  }

  StackSlot const* getSlots() {
    return profiler_.state_.stacks;
  }

  std::atomic<uint32_t>& getFullSlotsCounter() {
    return profiler_.state_.fullSlotsCounter;
  }

 private:
  SamplingProfiler& profiler_;
};

class SignalHandlerTestAccessor {
 public:
  static void AndroidAwareSigaction(
      int signum,
      SignalHandler::SigactionPtr handler,
      struct sigaction* oldact) {
    SignalHandler::AndroidAwareSigaction(signum, handler, oldact);
  }

  static bool isPhaserDraining(int signum) {
    return phaser_is_draining(
        &SignalHandler::globalRegisteredSignalHandlers[signum].load()->phaser_);
  }
};

long getTimeDifferenceUsec(timespec t1, timespec t2) {
  return (
      ((t1.tv_sec - t2.tv_sec) * kMicrosecondsInSecond) +
      (t1.tv_nsec - t2.tv_nsec) / kNanosecondsInMicrosecond);
}

void burnCpuMs(int workMilliseconds, float* f) {
  struct timespec start_time, curr_time;
  constexpr clockid_t clock_id = CLOCK_THREAD_CPUTIME_ID;
  const long workMicroseconds = workMilliseconds * kMicrosecondsInMillisecond;
  ASSERT_FALSE(clock_gettime(clock_id, &start_time));
  do {
    for (long ii = 0; ii < 1000000L; ii++) {
      *f += 1;
    }
    ASSERT_FALSE(clock_gettime(clock_id, &curr_time));
  } while (getTimeDifferenceUsec(curr_time, start_time) < workMicroseconds);
}

using TracerStdFunction = std::function<
    StackCollectionRetcode(ucontext_t*, int64_t*, uint16_t&, uint16_t)>;

class TestTracer : public BaseTracer {
 public:
  explicit TestTracer() {}

  void setCollectStackFn(std::unique_ptr<TracerStdFunction> fn) {
    collectStack_ = std::move(fn);
  }

  virtual StackCollectionRetcode collectStack(
      ucontext_t* ucontext,
      int64_t* frames,
      uint16_t& depth,
      uint16_t max_depth) {
    if (!collectStack_) {
      EXPECT_FALSE(true)
          << "Profiling callback hit before a tracer implementation has been registered";
      return StackCollectionRetcode::TRACER_DISABLED;
    }
    return (*collectStack_)(ucontext, frames, depth, max_depth);
  }

  virtual void flushStack(
      MultiBufferLogger& logger,
      int64_t* frames,
      uint16_t depth,
      int tid,
      int64_t time_) {}

  virtual void startTracing() {}

  virtual void stopTracing() {}

  // May be called to initialize static state. Must be always safe.
  virtual void prepare() {}

 private:
  std::unique_ptr<TracerStdFunction> collectStack_;
};

class SamplingProfilerTest : public ::testing::Test {
 protected:
  static constexpr int32_t kTestTracer = 0xfaceb00c;

  SamplingProfilerTest()
      : ::testing::Test(), tracer_(std::make_shared<TestTracer>()) {}

  static void SetUpTestCase() {
    struct sigaction act {};
    act.sa_handler = SIG_DFL;
    ASSERT_EQ(sigaction(SIGPROF, &act, nullptr), 0);
  }

  virtual void SetUp() {
    auto tracer_map =
        std::unordered_map<int32_t, std::shared_ptr<BaseTracer>>();

    // this is necessary because the map operator[] below takes a reference to
    // tracer_id
    auto tracer_id = kTestTracer;
    tracer_map[tracer_id] = tracer_;
    ASSERT_TRUE(profiler.initialize(logger, kTestTracer, tracer_map));
  }

  virtual void TearDown() {
    ASSERT_FALSE(access.isProfiling())
        << "Tests must finish in non-profiling state";
    tracer_->setCollectStackFn(std::unique_ptr<TracerStdFunction>(nullptr));
  }

  void SetTracer(std::unique_ptr<TracerStdFunction> tracer) {
    tracer_->setCollectStackFn(std::move(tracer));
  }

  void runLoggingTest(
      TracerStdFunction tracer,
      StackCollectionRetcode error,
      int expected_count = 1);

  void runLoggingTest(
      TracerStdFunction tracer,
      std::function<bool(StackSlot const&)> slot_predicate,
      int expected_count = 1);

  void runSampleCountTest(bool use_wall_time_profiling);

  void runThreadDetectTest(bool use_wall_time_profiling);

  void assertStopProfilingIsBlockingOnSignalHandler() {
    while (!access.isLoggerLoopDone()) {
      // Give the control thread a chance to enter stopProfiling(),
      // isLoggerLoopDone becoming true is part of the tear down, before we're
      // supposed to block.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // We inspect multiple signals, we want to be block on one of them.
    // Otherwise, we'd have to encode the order in which stopProfiling calls
    // SignalHandler::Disable here as well.
    bool sigsegv = SignalHandlerTestAccessor::isPhaserDraining(SIGSEGV);
    bool sigprof = SignalHandlerTestAccessor::isPhaserDraining(SIGPROF);

    ASSERT_TRUE(sigsegv || sigprof)
        << "Mandatory wait for signal handler to complete";

    ASSERT_TRUE(access.isProfiling());
  }

  SamplingProfiler profiler;
  MultiBufferLogger logger;
  SamplingProfilerTestAccessor access{profiler};

 private:
  std::shared_ptr<TestTracer> tracer_;
};

void SamplingProfilerTest::runLoggingTest(
    TracerStdFunction tracer,
    StackCollectionRetcode error,
    int expected_count) {
  runLoggingTest(
      tracer,
      [error](StackSlot const& slot) {
        return (slot.state.load() & 0xffff) == error;
      },
      expected_count);
}

void SamplingProfilerTest::runLoggingTest(
    TracerStdFunction tracer,
    std::function<bool(StackSlot const&)> slot_predicate,
    int expected_count) {
  // This test a SIGSEGV during tracing leads to an error entry being written.
  enum Sequence {
    START = 0,
    START_WORKER_THREAD,
    SEND_PROFILING_SIGNAL,
    START_TRACER,
    END_WORKER_THREAD,
    STOP_PROFILING,
    END,
  };
  test::TestSequencer sequencer{START, END};

  ASSERT_TRUE(profiler.startProfiling(
      kTestTracer,
      kDefaultSampleIntervalMs,
      kDefaultThreadDetectIntervalMs,
      kDefaultUseWallClockSetting));

  std::thread worker_thread([&] {
    sequencer.waitAndAdvance(START_WORKER_THREAD, SEND_PROFILING_SIGNAL);
    sequencer.waitAndAdvance(END_WORKER_THREAD, STOP_PROFILING);
  });
  auto tracer_wrapper =
      std::make_unique<TracerStdFunction>([&](ucontext_t* ucontext,
                                              int64_t* frames,
                                              uint16_t& depth,
                                              uint16_t max_depth) {
        sequencer.waitAndAdvance(START_TRACER, END_WORKER_THREAD);
        return tracer(ucontext, frames, depth, max_depth);
      });
  SetTracer(std::move(tracer_wrapper));

  // begin test
  sequencer.advance(START_WORKER_THREAD);

  sequencer.waitFor(SEND_PROFILING_SIGNAL);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(START_TRACER);

  sequencer.waitFor(STOP_PROFILING);
  profiler.stopProfiling();
  sequencer.advance(END);

  EXPECT_EQ(access.countSlotsWithPredicate(slot_predicate), expected_count)
      << "Incorrect number of slots matching the predicate";

  worker_thread.join();
}

TracerStdFunction signalCountTracerFunction(
    std::vector<int32_t>& tids,
    std::vector<int>& signal_cnt) {
  return [&signal_cnt, &tids](ucontext_t*, int64_t*, uint16_t&, uint16_t) {
    auto tid = threadID();
    for (int worker = 0; worker < tids.size(); worker++) {
      if (tid == tids[worker]) { // assumes tid != 0
        signal_cnt[worker]++;
        // FBLOGV(
        //     "----> signal worker %d: thread=%d cnt=%d",
        //     worker,
        //     tid,
        //     signal_cnt[worker]);
        break;
      }
    }
    // Falls thru if tid is not from a worker
    return SUCCESS;
  };
}

void assertSamplesWithinTolerance(
    int sample_interval_ms,
    int thread_detect_interval_ms,
    int allowed_lost_samples,
    std::vector<int> expected_times_ms,
    std::vector<int> signal_cnt) {
  for (int worker = 0; worker < signal_cnt.size(); worker++) {
    int expected_time_ms = expected_times_ms[worker];
    int signal_count_delta =
        abs(signal_cnt[worker] - expected_time_ms / sample_interval_ms);
    int tolerance =
        thread_detect_interval_ms / sample_interval_ms + allowed_lost_samples;
    std::stringstream ss;
    ss << "----> Thread: " << worker << " signals= " << signal_cnt[worker]
       << ", expected_time=" << expected_time_ms
       << ", expected_samples= " << expected_time_ms / sample_interval_ms
       << ", tolerance=" << tolerance;
    // FBLOGV("%s", ss.str().c_str());
    ASSERT_TRUE(signal_count_delta <= tolerance) << ss.str();
  }
}

void SamplingProfilerTest::runSampleCountTest(
    bool enable_wall_time_sampling) { // true->wall, false->CPU
  // This test runs two worker threads for different durations of time
  // to confirm the right number of signals are received for each thread.
  //
  // The worker threads pre-exist the logger_thread and startProfiling;
  // which means the sampler should detect the workers immediately, without
  // missing any samples.
  //
  // The workers restart working after stopProfiling(); the additional work
  // work should not be sampled.
  enum Sequence {
    START = 0,
    INIT_WORKER_THREAD0,
    INIT_WORKER_THREAD1,
    START_PROFILING,
    START_LOGGER,
    START_WORKER_THREAD0,
    START_WORKER_THREAD1,
    STOP_WORKER_THREAD0,
    STOP_WORKER_THREAD1,
    STOP_PROFILING,
    RESTART_WORKER_THREAD0,
    RESTART_WORKER_THREAD1,
    END,
  };
  test::TestSequencer sequencer{START, END};

  constexpr int num_workers = 2;
  std::array<int, num_workers> thread_cpu_ms = {
      750, 350}; // how much CPU to burn
  constexpr int post_sampling_thread_cpu_ms =
      100; // how much post-run CPU to burn
  constexpr int cpu_sample_interval_ms = 19;
  constexpr int wall_sample_interval_ms = 47;
  constexpr int allowed_lost_samples = 3;

  const int sample_interval_ms = enable_wall_time_sampling
      ? wall_sample_interval_ms
      : cpu_sample_interval_ms;

  const int thread_detect_interval_ms = sample_interval_ms;

  std::vector<int32_t> tids(num_workers, -1);
  std::vector<int> signal_cnt(num_workers, 0);

  sequencer.advance(INIT_WORKER_THREAD0);

  FBLOGV("Main thread is %d", threadID());

  // Worker 1
  std::thread worker_thread1([&] {
    auto tid = threadID();
    tids[0] = tid;
    if (enable_wall_time_sampling) {
      profiler.addToWhitelist(tid);
    }
    sequencer.waitAndAdvance(INIT_WORKER_THREAD0, INIT_WORKER_THREAD1);
    sequencer.waitAndAdvance(START_WORKER_THREAD0, START_WORKER_THREAD1);
    float f = 0;
    burnCpuMs(thread_cpu_ms[0], &f); // this work should get sampled
    sequencer.waitAndAdvance(STOP_WORKER_THREAD0, STOP_WORKER_THREAD1);
    sequencer.waitAndAdvance(RESTART_WORKER_THREAD0, RESTART_WORKER_THREAD1);
    burnCpuMs(post_sampling_thread_cpu_ms, &f); // this shouldn't get sampled
  });

  // Worker 2
  std::thread worker_thread2([&] {
    auto tid = threadID();
    tids[1] = tid;
    if (enable_wall_time_sampling) {
      profiler.addToWhitelist(tid);
    }
    sequencer.waitAndAdvance(INIT_WORKER_THREAD1, START_PROFILING);
    sequencer.waitAndAdvance(START_WORKER_THREAD1, STOP_WORKER_THREAD0);
    float f = 0;
    burnCpuMs(thread_cpu_ms[1], &f); // this work should get sampled
    sequencer.waitAndAdvance(STOP_WORKER_THREAD1, STOP_PROFILING);
    sequencer.waitAndAdvance(RESTART_WORKER_THREAD1, END);
    burnCpuMs(post_sampling_thread_cpu_ms, &f); // this shouldn't get sampled
  });

  // Tracer callback
  SetTracer(std::make_unique<TracerStdFunction>(
      signalCountTracerFunction(tids, signal_cnt)));

  sequencer.waitFor(START_PROFILING);
  ASSERT_TRUE(profiler.startProfiling(
      kTestTracer,
      sample_interval_ms,
      thread_detect_interval_ms,
      enable_wall_time_sampling));
  struct timespec start_time, end_time;
  ASSERT_FALSE(clock_gettime(CLOCK_MONOTONIC, &start_time));

  // Logger thread
  sequencer.advance(START_LOGGER);
  std::thread logger_thread([&] {
    sequencer.advance(START_WORKER_THREAD0);
    profiler.loggerLoop();
  });

  sequencer.waitFor(STOP_PROFILING);
  ASSERT_TRUE(access.isProfiling());
  ASSERT_FALSE(clock_gettime(CLOCK_MONOTONIC, &end_time));
  profiler.stopProfiling();
  ASSERT_FALSE(access.isProfiling());
  sequencer.advance(RESTART_WORKER_THREAD0);

  sequencer.waitFor(END);
  logger_thread.join();
  worker_thread2.join();
  worker_thread1.join();

  std::vector<int> expected_times_ms(num_workers, 0);
  for (int worker = 0; worker < num_workers; worker++) {
    expected_times_ms[worker] = enable_wall_time_sampling
        ? getTimeDifferenceUsec(end_time, start_time) /
            kMicrosecondsInMillisecond
        : thread_cpu_ms[worker];
  }
  assertSamplesWithinTolerance(
      sample_interval_ms,
      0, // thread detect is @start - don't consider thread_detect_interval_ms
      allowed_lost_samples,
      expected_times_ms,
      signal_cnt);
} // namespace profiler

void SamplingProfilerTest::runThreadDetectTest(
    bool enable_wall_time_sampling) { // true: wall, false; CPU
  // This test confirms that the thread profiler detects newly created threads
  // and that sampling continues without errors as threads are added/removed.
  enum Sequence {
    START = 0,
    START_LOGGER,
    START_PROFILING,
    RUN_WORKERS,
    STOP_PROFILING,
    END,
  };
  test::TestSequencer sequencer{START, END};

  constexpr int num_iterations = 3;
  constexpr int num_parallel_threads = 3;
  constexpr int num_workers = num_iterations * num_parallel_threads;
  const std::vector<int> thread_cpu_ms(
      num_workers, 300); // how much CPU to burn
  constexpr int cpu_sample_interval_ms = 19;
  constexpr int wall_sample_interval_ms = 47;
  constexpr int allowed_lost_samples = 3;

  const int sample_interval_ms = enable_wall_time_sampling
      ? wall_sample_interval_ms
      : cpu_sample_interval_ms;

  const int thread_detect_interval_ms = sample_interval_ms;

  std::vector<int32_t> tids(num_workers, -1);
  std::vector<int> signal_cnt(num_workers, 0);
  std::vector<struct timespec> start_time(num_workers, {0});
  std::vector<struct timespec> end_time(num_workers, {0});

  // Tracer callback
  SetTracer(std::make_unique<TracerStdFunction>(
      signalCountTracerFunction(tids, signal_cnt)));

  // Logger thread
  sequencer.advance(START_LOGGER);
  std::thread logger_thread([&sequencer, this] {
    sequencer.advance(START_PROFILING);
    this->profiler.loggerLoop();
  });

  sequencer.waitFor(START_PROFILING);
  ASSERT_TRUE(profiler.startProfiling(
      kTestTracer,
      sample_interval_ms,
      thread_detect_interval_ms,
      enable_wall_time_sampling));
  sequencer.advance(RUN_WORKERS);

  // FBLOGV("------> main thread is %d", threadID());

  // Worker threads (blocking)
  for (int worker = 0, itr = 0; itr < num_iterations; itr++) {
    // FBLOGV("-------> START OF GROUP %d", itr);
    std::thread threads[num_parallel_threads];
    for (int j = 0; j < num_parallel_threads; j++, worker++) {
      threads[j] = std::thread([worker,
                                this,
                                &enable_wall_time_sampling,
                                &tids,
                                &thread_cpu_ms,
                                &end_time,
                                &start_time] {
        auto tid = threadID();
        if (enable_wall_time_sampling) {
          profiler.addToWhitelist(tid);
        }
        // FBLOGV("----> thread started %d:%d", worker, tid);
        tids[worker] = tid;
        ASSERT_FALSE(clock_gettime(CLOCK_MONOTONIC, &start_time[worker]));
        float f = 0;
        burnCpuMs(thread_cpu_ms[worker], &f);
        ASSERT_FALSE(clock_gettime(CLOCK_MONOTONIC, &end_time[worker]));
      });
    }
    for (int j = 0; j < num_parallel_threads; j++) {
      threads[j].join();
    }
  }

  sequencer.advance(STOP_PROFILING);
  ASSERT_TRUE(access.isProfiling());
  profiler.stopProfiling();
  ASSERT_FALSE(access.isProfiling());

  logger_thread.join();
  sequencer.advance(END);

  std::vector<int> expected_times_ms(num_workers, 0);
  for (int worker = 0; worker < num_workers; worker++) {
    expected_times_ms[worker] = enable_wall_time_sampling
        ? getTimeDifferenceUsec(end_time[worker], start_time[worker]) /
            kMicrosecondsInMillisecond
        : thread_cpu_ms[worker];
  }
  assertSamplesWithinTolerance(
      sample_interval_ms,
      thread_detect_interval_ms,
      allowed_lost_samples,
      expected_times_ms,
      signal_cnt);
}

TEST_F(SamplingProfilerTest, errorLoggingFaultDuringTracing) {
  runLoggingTest(
      [](ucontext_t*, int64_t*, uint16_t&, uint16_t) {
        raise(SIGSEGV);
        return StackCollectionRetcode::SUCCESS;
      },
      StackCollectionRetcode::SIGNAL_INTERRUPT);
}

TEST_F(SamplingProfilerTest, errorLoggingEmptyStack) {
  runLoggingTest(
      [](ucontext_t*, int64_t*, uint16_t& depth, uint16_t) {
        return StackCollectionRetcode::EMPTY_STACK;
      },
      StackCollectionRetcode::EMPTY_STACK);
}

TEST_F(SamplingProfilerTest, errorLoggingNoStackForThread) {
  runLoggingTest(
      [](ucontext_t*, int64_t*, uint16_t& depth, uint16_t) {
        return StackCollectionRetcode::NO_STACK_FOR_THREAD;
      },
      StackCollectionRetcode::NO_STACK_FOR_THREAD);
}

TEST_F(SamplingProfilerTest, errorLoggingStackOverflow) {
  runLoggingTest(
      [](ucontext_t*, int64_t*, uint16_t& depth, uint16_t) {
        return StackCollectionRetcode::STACK_OVERFLOW;
      },
      StackCollectionRetcode::STACK_OVERFLOW);
}

TEST_F(SamplingProfilerTest, noErrorLoggingForTracerDisabled) {
  runLoggingTest(
      [](ucontext_t*, int64_t*, uint16_t& depth, uint16_t) {
        return StackCollectionRetcode::TRACER_DISABLED;
      },
      StackCollectionRetcode::TRACER_DISABLED,
      0);
}

TEST_F(SamplingProfilerTest, noErrorLoggingForTracerIgnoreRetcode) {
  auto& fullStacksBefore = access.getFullSlotsCounter();
  runLoggingTest(
      [](ucontext_t*, int64_t*, uint16_t& depth, uint16_t) {
        return StackCollectionRetcode::IGNORE;
      },
      StackCollectionRetcode::IGNORE,
      0);
  ASSERT_EQ(access.getFullSlotsCounter().load(), fullStacksBefore.load());
}

TEST_F(SamplingProfilerTest, basicStackLogging) {
  constexpr int64_t MAGIC_FRAME = 0xfaceb00c;
  runLoggingTest(
      [](ucontext_t*, int64_t* frames, uint16_t& depth, uint16_t max_depth) {
        for (int i = 0; i < max_depth; ++i) {
          frames[i] = MAGIC_FRAME;
        }
        depth = max_depth;
        return StackCollectionRetcode::SUCCESS;
      },
      [](StackSlot const& slot) {
        if ((slot.state.load() & 0xffff) != StackCollectionRetcode::SUCCESS) {
          return false;
        }
        if (slot.depth != MAX_STACK_DEPTH) {
          return false;
        }
        if (slot.profilerType != kTestTracer) {
          return false;
        }
        for (int i = 0; i < slot.depth; ++i) {
          if (slot.frames[i] != MAGIC_FRAME) {
            return false;
          }
        }
        return true;
      });
}

TEST_F(SamplingProfilerTest, stopProfilingWhileHandlingFault) {
  // This test ensures that stopProfiling waits for currently executing fault
  // handlers to finish before returning. If that's not the case, the test will
  // sporadically fail.

  enum Sequence {
    START = 0,
    START_PROFILING,
    START_WORKER_THREAD,
    REGISTER_FAULT_HANDLER,

    SEND_PROFILING_SIGNAL,
    START_FAULT_HANDLER,
    INSPECT_PRE_STOP,

    STOP_PROFILING,
    INSPECT_MIDDLE_OF_STOP,
    END_FAULT_HANDLER,

    HAS_STOPPED_PROFILING,
    INSPECT_POST_STOP,
    END_WORKER_THREAD,

    END,
  };
  test::TestSequencer sequencer{START, END};

  // Control thread to start and stop the sampling profiler.
  // Can't be the main thread because we want to verify that the thread blocks.
  std::thread control_thread([&] {
    sequencer.waitFor(START_PROFILING);
    ASSERT_TRUE(profiler.startProfiling(
        kTestTracer,
        kDefaultSampleIntervalMs,
        kDefaultThreadDetectIntervalMs,
        kDefaultUseWallClockSetting));
    sequencer.advance(START_WORKER_THREAD);

    sequencer.waitAndAdvance(STOP_PROFILING, INSPECT_MIDDLE_OF_STOP);
    profiler.stopProfiling();
    sequencer.waitAndAdvance(HAS_STOPPED_PROFILING, INSPECT_POST_STOP);
  });

  // Target thread that will receive the profiling signal.
  std::thread worker_thread([&] {
    sequencer.waitAndAdvance(START_WORKER_THREAD, REGISTER_FAULT_HANDLER);

    sequencer.waitAndAdvance(END_WORKER_THREAD, END);
  });

  // Tracer implementation that just raises SIGSEGV.
  auto tracer_implementation = std::make_unique<TracerStdFunction>(
      [&](ucontext_t*, int64_t*, uint16_t&, uint16_t) {
        raise(SIGSEGV);
        return SUCCESS;
      });
  SetTracer(std::move(tracer_implementation));

  // Begin the test here.
  sequencer.advance(START_PROFILING);

  sequencer.waitFor(REGISTER_FAULT_HANDLER);

  // Register a SIGSEGV handler that takes over the signal slot
  // by calling sigaction directly.
  //
  // We need to register this handler *after* profiling has
  // started to be able to execute *before* the fault handler from
  // SamplingProfiler.
  struct fault_handler_state {
    test::TestSequencer* testSequencer;
    struct sigaction oldaction;
  };
  // This needs to be static to be accessible from the
  // non-capturing lambda that serves as a signal handler below.
  static struct fault_handler_state handler_state {};
  handler_state = fault_handler_state{
      .testSequencer = &sequencer,
  };

  SignalHandlerTestAccessor::AndroidAwareSigaction(
      SIGSEGV,
      +[](int signum, siginfo_t* siginfo, void* ucontext) {
        handler_state.testSequencer->waitAndAdvance(
            START_FAULT_HANDLER, INSPECT_PRE_STOP);

        handler_state.testSequencer->waitAndAdvance(
            END_FAULT_HANDLER, HAS_STOPPED_PROFILING);

        // Call SamplingProfiler's handler
        handler_state.oldaction.sa_sigaction(signum, siginfo, ucontext);
      },
      &handler_state.oldaction);

  sequencer.advance(SEND_PROFILING_SIGNAL);

  sequencer.waitFor(SEND_PROFILING_SIGNAL);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(START_FAULT_HANDLER);

  sequencer.waitFor(INSPECT_PRE_STOP);
  ASSERT_TRUE(access.isProfiling());
  sequencer.advance(STOP_PROFILING);

  sequencer.waitFor(INSPECT_MIDDLE_OF_STOP);
  assertStopProfilingIsBlockingOnSignalHandler();

  // Commenting out this line should block the test forever
  sequencer.advance(END_FAULT_HANDLER);

  sequencer.waitFor(INSPECT_POST_STOP);
  ASSERT_FALSE(access.isProfiling());
  sequencer.advance(END_WORKER_THREAD);

  control_thread.join();
  worker_thread.join();

  SignalHandlerTestAccessor::AndroidAwareSigaction(
      SIGSEGV, handler_state.oldaction.sa_sigaction, nullptr);
}

TEST_F(SamplingProfilerTest, stopProfilingWhileExecutingTracer) {
  // This test ensures that stopProfiling waits for currently executing
  // profiling handlers to finish before returning.

  enum Sequence {
    START = 0,
    START_PROFILING,
    START_WORKER_THREAD,
    SEND_PROFILING_SIGNAL,
    START_TRACER_CALL,

    INSPECT_PRE_STOP,
    STOP_PROFILING,
    INSPECT_MIDDLE_OF_STOP,
    END_TRACER_CALL,
    HAS_STOPPED_PROFILING,
    INSPECT_POST_STOP,

    END_WORKER_THREAD,
    END,
  };
  test::TestSequencer sequencer{START, END};

  // Control thread to start and stop the sampling profiler.
  // Can't be the main thread because we want to verify that the thread blocks.
  std::thread control_thread([&] {
    sequencer.waitFor(START_PROFILING);
    ASSERT_TRUE(profiler.startProfiling(
        kTestTracer,
        kDefaultSampleIntervalMs,
        kDefaultThreadDetectIntervalMs,
        kDefaultUseWallClockSetting));
    sequencer.advance(START_WORKER_THREAD);

    sequencer.waitAndAdvance(STOP_PROFILING, INSPECT_MIDDLE_OF_STOP);
    profiler.stopProfiling();
    sequencer.waitAndAdvance(HAS_STOPPED_PROFILING, INSPECT_POST_STOP);
  });

  // Target thread that will receive the profiling signal.
  std::thread worker_thread([&] {
    sequencer.waitAndAdvance(START_WORKER_THREAD, SEND_PROFILING_SIGNAL);

    sequencer.waitAndAdvance(END_WORKER_THREAD, END);
  });

  auto tracer_implementation = std::make_unique<TracerStdFunction>(
      [&](ucontext_t*, int64_t*, uint16_t&, uint16_t) {
        sequencer.waitAndAdvance(START_TRACER_CALL, INSPECT_PRE_STOP);

        sequencer.waitAndAdvance(END_TRACER_CALL, HAS_STOPPED_PROFILING);
        return SUCCESS;
      });
  SetTracer(std::move(tracer_implementation));

  // Begin the test here.
  sequencer.advance(START_PROFILING);

  sequencer.waitFor(SEND_PROFILING_SIGNAL);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(START_TRACER_CALL);

  sequencer.waitFor(INSPECT_PRE_STOP);
  ASSERT_TRUE(access.isProfiling());
  sequencer.advance(STOP_PROFILING);

  sequencer.waitFor(INSPECT_MIDDLE_OF_STOP);

  assertStopProfilingIsBlockingOnSignalHandler();

  // Commenting out this line should block the test forever
  sequencer.advance(END_TRACER_CALL);

  sequencer.waitFor(INSPECT_POST_STOP);
  ASSERT_FALSE(access.isProfiling());
  sequencer.advance(END_WORKER_THREAD);

  control_thread.join();
  worker_thread.join();
}

TEST_F(SamplingProfilerTest, nestedFaultingTracersUnstackProperly) {
  // This test ensures that 3 nested tracer calls on the same thread handle
  // their faults in the right order (most recent first).

  enum Sequence {
    START = 0,

    START_WORKER_THREAD,

    SEND_PROFILING_SIGNAL,
    TRACER_CALL_1,
    START_FAULT_HANDLER_1,

    SEND_PROFILING_SIGNAL2,
    TRACER_CALL_2,
    START_FAULT_HANDLER_2,

    SEND_PROFILING_SIGNAL3,
    TRACER_CALL_3,
    START_FAULT_HANDLER_3,

    // All handlers are on the stack, pop them one by one.
    END_FAULT_HANDLER_3,
    END_FAULT_HANDLER_2,
    END_FAULT_HANDLER_1,

    STOP_PROFILING,

    END_WORKER_THREAD,
    END,
  };
  test::TestSequencer sequencer{START, END};

  ASSERT_TRUE(profiler.startProfiling(
      kTestTracer,
      kDefaultSampleIntervalMs,
      kDefaultThreadDetectIntervalMs,
      kDefaultUseWallClockSetting));

  // Target thread that will receive the profiling signal.
  std::thread worker_thread([&] {
    sequencer.waitAndAdvance(START_WORKER_THREAD, SEND_PROFILING_SIGNAL);

    sequencer.waitAndAdvance(END_WORKER_THREAD, END);
  });

  std::atomic_int num_started_tracers{0};
  // Tracer implementation that just spins on an atomic variable.
  auto tracer_implementation = std::make_unique<TracerStdFunction>(
      [&](ucontext_t*, int64_t*, uint16_t&, uint16_t) {
        auto tracer_idx = num_started_tracers++;
        auto turn = 0, next = 0;
        switch (tracer_idx) {
          case 0: {
            turn = TRACER_CALL_1;
            next = START_FAULT_HANDLER_1;
            break;
          }
          case 1: {
            turn = TRACER_CALL_2;
            next = START_FAULT_HANDLER_2;
            break;
          }
          case 2: {
            turn = TRACER_CALL_3;
            next = START_FAULT_HANDLER_3;
            break;
          }
        }
        sequencer.waitAndAdvance(turn, next);
        raise(SIGSEGV);
        return SUCCESS;
      });
  SetTracer(std::move(tracer_implementation));

  // Register a SIGSEGV handler that takes over the signal slot
  // by calling sigaction directly.
  //
  // We need to register this handler *after* profiling has
  // started to be able to execute *before* the fault handler from
  // SamplingProfiler.
  //
  struct fault_handler_state {
    test::TestSequencer* testSequencer;
    std::atomic_int* num_started_tracer_calls;
    std::atomic_int num_started_fault_handlers;
    struct sigaction oldaction;
  };
  // This needs to be static to be accessible from the
  // non-capturing lambda that serves as a signal handler below.
  static struct fault_handler_state handler_state {};
  handler_state.testSequencer = &sequencer;
  handler_state.num_started_tracer_calls = &num_started_tracers;

  SignalHandlerTestAccessor::AndroidAwareSigaction(
      SIGSEGV,
      [](int signum, siginfo_t* siginfo, void* ucontext) {
        auto handler_idx = handler_state.num_started_fault_handlers++;

        auto start_turn = 0;
        auto end_turn = 0;
        auto start_advance_to_turn = 0;
        auto end_advance_to_turn = 0;
        switch (handler_idx) {
          case 0: {
            start_turn = START_FAULT_HANDLER_1;
            start_advance_to_turn = SEND_PROFILING_SIGNAL2;

            end_turn = END_FAULT_HANDLER_1;
            end_advance_to_turn = STOP_PROFILING;
            EXPECT_EQ(handler_state.num_started_tracer_calls->load(), 1);
            break;
          }
          case 1: {
            start_turn = START_FAULT_HANDLER_2;
            start_advance_to_turn = SEND_PROFILING_SIGNAL3;

            end_turn = END_FAULT_HANDLER_2;
            end_advance_to_turn = END_FAULT_HANDLER_1;
            EXPECT_EQ(handler_state.num_started_tracer_calls->load(), 2);
            break;
          }
          case 2: {
            start_turn = START_FAULT_HANDLER_3;
            start_advance_to_turn = END_FAULT_HANDLER_3;

            end_turn = END_FAULT_HANDLER_3;
            end_advance_to_turn = END_FAULT_HANDLER_2;
            EXPECT_EQ(handler_state.num_started_tracer_calls->load(), 3);
            break;
          }
        }
        handler_state.testSequencer->waitAndAdvance(
            start_turn, start_advance_to_turn);
        //
        // We want the exit times from the fault handler to be at least 1 ms
        // apart, so we can use strict inequality comparisons when examining the
        // timestamps.
        //
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        handler_state.testSequencer->waitAndAdvance(
            end_turn, end_advance_to_turn);

        // Call SamplingProfiler's handler
        handler_state.oldaction.sa_sigaction(signum, siginfo, ucontext);
      },
      &handler_state.oldaction);

  // Begin the test here.
  sequencer.advance(START_WORKER_THREAD);

  sequencer.waitFor(SEND_PROFILING_SIGNAL);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(TRACER_CALL_1);

  sequencer.waitFor(SEND_PROFILING_SIGNAL2);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(TRACER_CALL_2);

  sequencer.waitFor(SEND_PROFILING_SIGNAL3);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(TRACER_CALL_3);

  sequencer.waitAndAdvance(STOP_PROFILING, END_WORKER_THREAD);
  profiler.stopProfiling();

  auto numErrors = access.countSlotsWithPredicate([](StackSlot const& slot) {
    return (slot.state.load() & 0xffff) ==
        StackCollectionRetcode::SIGNAL_INTERRUPT &&
        slot.profilerType == kTestTracer;
  });
  EXPECT_EQ(numErrors, 3);

  // The earliest slot should belong to the earliest entry to the tracer.
  // However, signal errors update the time slot with the time of return from
  // the fault handler. Therefore, the earliest slot should exit last and have
  // the highest timestamp. We can use strict inequality because we arrange the
  // exit times to be at least 1ms apart.
  EXPECT_GT(access.getSlots()[0].time, access.getSlots()[1].time);
  EXPECT_GT(access.getSlots()[1].time, access.getSlots()[2].time);

  worker_thread.join();

  SignalHandlerTestAccessor::AndroidAwareSigaction(
      SIGSEGV, handler_state.oldaction.sa_sigaction, nullptr);
}

TEST_F(SamplingProfilerTest, profilingSignalIsIgnoredAfterStop) {
  // This test ensures that a pending SIGPROF at the time of stopProfiling, when
  // delivered after stopProfiling, does not take down the process.
  //
  // While we can't really manipulate the pending and delivered state at that
  // granularity, we observe that from the point of view of SamplingProfiler,
  // this is equivalent to a signal sent-and-delivered entirely after
  // stopProfiling.

  ASSERT_TRUE(profiler.startProfiling(
      kTestTracer,
      kDefaultSampleIntervalMs,
      kDefaultThreadDetectIntervalMs,
      kDefaultUseWallClockSetting));
  profiler.stopProfiling();

  // No death!
  pthread_kill(pthread_self(), SIGPROF);
}

TEST_F(SamplingProfilerTest, verifyCpuSampleCounts) {
  runSampleCountTest(false);
}

TEST_F(SamplingProfilerTest, verifyWallSampleCounts) {
  runSampleCountTest(true);
}

TEST_F(SamplingProfilerTest, verifyCpuThreadDetect) {
  runThreadDetectTest(false);
}

TEST_F(SamplingProfilerTest, verifyWallThreadDetect) {
  runThreadDetectTest(true);
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
