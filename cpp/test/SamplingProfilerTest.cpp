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
#include <inttypes.h>
#include <signal.h>
#include <atomic>
#include <thread>

#include <profiler/SamplingProfiler.h>
#include <profilo/test/TestSequencer.h>

namespace facebook {
namespace profilo {
namespace profiler {

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

 private:
  SamplingProfiler& profiler_;
};

using TracerStdFunction = std::function<
    StackCollectionRetcode(ucontext_t*, int64_t*, uint8_t&, uint8_t)>;

class TestTracer : public BaseTracer {
 public:
  explicit TestTracer() {}

  void setCollectStackFn(std::unique_ptr<TracerStdFunction> fn) {
    collectStack_ = std::move(fn);
  }

  virtual StackCollectionRetcode collectStack(
      ucontext_t* ucontext,
      int64_t* frames,
      uint8_t& depth,
      uint8_t max_depth) {
    if (!collectStack_) {
      EXPECT_FALSE(true)
          << "Profiling callback hit before a tracer implementation has been registered";
      return StackCollectionRetcode::TRACER_DISABLED;
    }
    return (*collectStack_)(ucontext, frames, depth, max_depth);
  }

  virtual void
  flushStack(int64_t* frames, uint8_t depth, int tid, int64_t time_) {}

  virtual void startTracing() {}

  virtual void stopTracing() {}

  // May be called to initialize static state. Must be always safe.
  virtual void prepare() {}

 private:
  std::unique_ptr<TracerStdFunction> collectStack_;
};

class SamplingProfilerTest : public ::testing::Test {
 protected:
  static constexpr int32_t kTestTracer = 1;

  SamplingProfilerTest()
      : ::testing::Test(), tracer_(std::make_shared<TestTracer>()) {}

  static void SetUpTestCase() {
    struct sigaction act {
      .sa_handler = SIG_DFL,
    };
    ASSERT_EQ(sigaction(SIGPROF, &act, nullptr), 0);
  }

  virtual void SetUp() {
    auto tracer_map =
        std::unordered_map<int32_t, std::shared_ptr<BaseTracer>>();

    // this is necessary because the map operator[] below takes a reference to
    // tracer_id
    auto tracer_id = kTestTracer;
    tracer_map[tracer_id] = tracer_;
    ASSERT_TRUE(profiler.initialize(kTestTracer, tracer_map));
  }

  virtual void TearDown() {
    ASSERT_FALSE(access.isProfiling())
        << "Tests must finish in non-profiling state";
    tracer_->setCollectStackFn(std::unique_ptr<TracerStdFunction>(nullptr));
  }

  void SetTracer(std::unique_ptr<TracerStdFunction> tracer) {
    tracer_->setCollectStackFn(std::move(tracer));
  }

  SamplingProfiler profiler;
  SamplingProfilerTestAccessor access{profiler};

 private:
  std::shared_ptr<TestTracer> tracer_;
};

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
    ASSERT_TRUE(profiler.startProfiling(kTestTracer, 1800 * 1000, false));
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
      [&](ucontext_t*, int64_t*, uint8_t&, uint8_t) {
        raise(SIGSEGV);
        return SUCCESS;
      });
  SetTracer(std::move(tracer_implementation));

  // Begin the test here.
  sequencer.advance(START_PROFILING);

  sequencer.waitFor(REGISTER_FAULT_HANDLER);

  // Register a SIGSEGV handler that participates in the global test order.
  // This relies on a sigmux implementation detail - handlers are
  // prepended to the list. Therefore, we need to register this handler
  // *after* profiling has started to execute *before* the fault handler from
  // SamplingProfiler.
  struct fault_handler_state {
    test::TestSequencer& testSequencer;
  } handler_state{
      .testSequencer = sequencer,
  };

  sigset_t sigsegv{};
  sigaddset(&sigsegv, SIGSEGV);
  auto sigmux_registration = sigmux_register(
      &sigsegv,
      +[](sigmux_siginfo*, void* data) {
        auto state = (fault_handler_state*)data;
        state->testSequencer.waitAndAdvance(
            START_FAULT_HANDLER, INSPECT_PRE_STOP);

        state->testSequencer.waitAndAdvance(
            END_FAULT_HANDLER, HAS_STOPPED_PROFILING);
        return SIGMUX_CONTINUE_SEARCH;
      },
      &handler_state,
      0);

  ASSERT_NE(sigmux_registration, nullptr);
  sequencer.advance(SEND_PROFILING_SIGNAL);

  sequencer.waitFor(SEND_PROFILING_SIGNAL);
  pthread_kill(worker_thread.native_handle(), SIGPROF);
  sequencer.advance(START_FAULT_HANDLER);

  sequencer.waitFor(INSPECT_PRE_STOP);
  ASSERT_TRUE(access.isProfiling());
  sequencer.advance(STOP_PROFILING);

  sequencer.waitFor(INSPECT_MIDDLE_OF_STOP);
  ASSERT_TRUE(access.isProfiling())
      << "Still profiling as we haven't exited the tracer loop";

  // Commenting out this line should block the test forever
  sequencer.advance(END_FAULT_HANDLER);

  sequencer.waitFor(INSPECT_POST_STOP);
  ASSERT_FALSE(access.isProfiling());
  sequencer.advance(END_WORKER_THREAD);

  control_thread.join();
  worker_thread.join();

  sigmux_unregister(sigmux_registration);
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
    ASSERT_TRUE(profiler.startProfiling(kTestTracer, 1800 * 1000, false));
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
      [&](ucontext_t*, int64_t*, uint8_t&, uint8_t) {
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

  while (!access.isLoggerLoopDone()) {
    // Give the control thread a chance to enter stopProfiling(),
    // isLoggerLoopDone becoming true is part of the tear down, before we're
    // supposed to block.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  sequencer.waitFor(INSPECT_MIDDLE_OF_STOP);
  ASSERT_TRUE(access.isProfiling())
      << "Still profiling as we haven't exited the tracer loop";

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

  ASSERT_TRUE(profiler.startProfiling(kTestTracer, 1800 * 1000, false));

  // Target thread that will receive the profiling signal.
  std::thread worker_thread([&] {
    sequencer.waitAndAdvance(START_WORKER_THREAD, SEND_PROFILING_SIGNAL);

    sequencer.waitAndAdvance(END_WORKER_THREAD, END);
  });

  std::atomic_int num_started_tracers{0};
  // Tracer implementation that just spins on an atomic variable.
  auto tracer_implementation = std::make_unique<TracerStdFunction>(
      [&](ucontext_t*, int64_t*, uint8_t&, uint8_t) {
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

  // Register a SIGSEGV handler that participates in the global test order.
  // This relies on a sigmux implementation detail - handlers are
  // prepended to the list. Therefore, we need to register this handler
  // *after* profiling has started to execute *before* the fault handler from
  // SamplingProfiler.
  struct fault_handler_state {
    test::TestSequencer& testSequencer;
    std::atomic_int& num_started_tracer_calls;
    std::atomic_int num_started_fault_handlers;
  } handler_state{
      .testSequencer = sequencer,
      .num_started_tracer_calls = num_started_tracers,
  };

  sigset_t sigsegv{};
  sigaddset(&sigsegv, SIGSEGV);
  auto sigmux_registration = sigmux_register(
      &sigsegv,
      +[](sigmux_siginfo*, void* data) {
        auto state = (fault_handler_state*)data;
        auto handler_idx = state->num_started_fault_handlers++;

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
            EXPECT_EQ(state->num_started_tracer_calls.load(), 1);
            break;
          }
          case 1: {
            start_turn = START_FAULT_HANDLER_2;
            start_advance_to_turn = SEND_PROFILING_SIGNAL3;

            end_turn = END_FAULT_HANDLER_2;
            end_advance_to_turn = END_FAULT_HANDLER_1;
            EXPECT_EQ(state->num_started_tracer_calls.load(), 2);
            break;
          }
          case 2: {
            start_turn = START_FAULT_HANDLER_3;
            start_advance_to_turn = END_FAULT_HANDLER_3;

            end_turn = END_FAULT_HANDLER_3;
            end_advance_to_turn = END_FAULT_HANDLER_2;
            EXPECT_EQ(state->num_started_tracer_calls.load(), 3);
            break;
          }
        }
        state->testSequencer.waitAndAdvance(start_turn, start_advance_to_turn);
        //
        // We want the exit times from the fault handler to be at least 1 ms
        // apart, so we can use strict inequality comparisons when examining the
        // timestamps.
        //
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        state->testSequencer.waitAndAdvance(end_turn, end_advance_to_turn);
        return SIGMUX_CONTINUE_SEARCH;
      },
      &handler_state,
      0);

  ASSERT_NE(sigmux_registration, nullptr);

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

  sigmux_unregister(sigmux_registration);
}

TEST_F(SamplingProfilerTest, profilingSignalIsIgnoredAfterStop) {
  // This test ensures that a pending SIGPROF at the time of stopProfiling, when
  // delivered after stopProfiling, does not take down the process.
  //
  // While we can't really manipulate the pending and delivered state at that
  // granularity, we observe that from the point of view of SamplingProfiler,
  // this is equivalent to a signal sent-and-delivered entirely after
  // stopProfiling.
  struct sigaction act {
    .sa_handler = SIG_DFL,
  };
  ASSERT_EQ(sigmux_sigaction(SIGPROF, &act, nullptr), 0);

  // SIG_DFL for SIGPROF is Term, we should die on SIGPROF.
  ASSERT_DEATH(pthread_kill(pthread_self(), SIGPROF), "");

  ASSERT_TRUE(profiler.startProfiling(kTestTracer, 1800 * 1000, false));
  profiler.stopProfiling();

  // No death!
  pthread_kill(pthread_self(), SIGPROF);
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
