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

#include "TimerManager.h"

#include <semaphore.h>
#include <setjmp.h>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#if !defined(__GLIBCXX__)
#include <__threading_support>
#endif

#include <fbjni/fbjni.h>
#include <profiler/BaseTracer.h>
#include <profiler/Constants.h>
#include <profiler/SignalHandler.h>
#include <profilo/ExternalApiGlue.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace profiler {

enum StackSlotState {
  FREE = StackCollectionRetcode::MAXVAL + 1,
  BUSY,
  BUSY_WITH_METADATA,
};

//
// Slots are preallocated storage for the sampling profiler. They
// are necessary because unwinding happens in a signal context and thus
// allocation via the traditional APIs is not possible.
//
// The slot state encodes the tid in the high 16 bits and the
// state (StackSlotState) in the lower 16 bits.
//
// Each slot goes through a lifecycle:
//   FREE -> BUSY -> BUSY_WITH_METADATA -> {StackCollectionRetcode}
//
struct StackSlot {
  std::atomic<uint64_t> state;
  uint8_t depth;
  int64_t time;
  sigjmp_buf sig_jmp_buf;
  uint32_t profilerType;
  int64_t frames[MAX_STACK_DEPTH]; // frame pointer addresses
  char const* method_names[MAX_STACK_DEPTH];
  char const* class_descriptors[MAX_STACK_DEPTH];
#ifdef PROFILER_COLLECT_PC
  u2 pcs[MAX_STACK_DEPTH];
#endif

  StackSlot() : state(StackSlotState::FREE), depth(0) {}
};

struct Whitelist {
  std::unordered_set<int32_t> whitelistedThreads;
  std::mutex whitelistedThreadsMtx; // Guards whitelistedThreads
};

struct ProfileState {
  pid_t processId;
  int availableTracers;
  int currentTracers;
  std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracersMap;
  int64_t profileStartTime;
  std::atomic_bool isProfiling{};

  // Slots/Stacks
  StackSlot stacks[MAX_STACKS_COUNT];
  std::atomic<uint32_t> currentSlot;
  std::atomic<uint32_t> fullSlotsCounter;

  // Error stats
  std::atomic<uint16_t> errSigCrashes;
  std::atomic<uint16_t> errSlotMisses;
  std::atomic<uint16_t> errStackOverflows;

  // Logger
  sem_t slotsCounterSem;
  std::atomic_bool isLoggerLoopDone;

  // Config parameters
  bool wallClockModeEnabled;
  int threadDetectIntervalMs;
  int samplingRateMs;

  // When in "wall clock mode", we can optionally whitelist additional threads
  // to profile as well.
  std::shared_ptr<Whitelist> whitelist;

  std::unique_ptr<TimerManager> timerManager;

  // If a secondary trace starts, we need to tell the logger loop to clear
  // its cache of logged frames, so that the new trace won't miss any symbols
  std::atomic_bool resetFrameworkSymbols;
};

/**
 * Exported functions
 */

class SamplingProfiler {
 public:
  static SamplingProfiler& getInstance();

  SamplingProfiler() : state_() {
    state_.whitelist = std::make_shared<Whitelist>();
  }

  bool initialize(
      int32_t available_tracers,
      std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracers);

  void loggerLoop();

  void stopProfiling();

  bool startProfiling(
      int requested_providers,
      int sampling_rate_ms,
      int thread_detect_interval_ms,
      bool wall_clock_mode_enabled);

  void addToWhitelist(int targetThread);

  void removeFromWhitelist(int targetThread);

  void resetFrameworkNamesSet();

  static std::unordered_map<int32_t, std::shared_ptr<BaseTracer>>
  ComputeAvailableTracers(uint32_t available_tracers);

 private:
  ProfileState state_;
  struct {
    SignalHandler* sigsegv;
    SignalHandler* sigbus;
    SignalHandler* sigprof;
  } signal_handlers_;

  // Profiling timer management
  bool startProfilingTimers();
  bool stopProfilingTimers();

  void registerSignalHandlers();
  void unregisterSignalHandlers();

  // Logger
  void maybeSignalReader();
  void flushStackTraces(std::unordered_set<uint64_t>& loggedFramesSet);

  static void FaultHandler(int, siginfo_t*, void*);
  static void UnwindStackHandler(int, siginfo_t*, void*);

  friend class SamplingProfilerTestAccessor;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
