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

#include <semaphore.h>
#include <setjmp.h>
#include <sigmux.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#if !defined(__GLIBCXX__)
#include <__threading_support>
#endif

#include <fbjni/fbjni.h>

#include <profiler/Constants.h>

#include "DalvikTracer.h"
#include "profilo/LogEntry.h"
#include "util/ProcFs.h"

#include <profilo/ExternalApi.h>

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
  std::atomic<uint32_t> state;
  uint8_t depth;
  int64_t time;
  jmp_buf sig_jmp_buf;
  uint32_t profilerType;
  int64_t frames[MAX_STACK_DEPTH]; // frame pointer addresses
  char const* method_names[MAX_STACK_DEPTH];
  char const* class_descriptors[MAX_STACK_DEPTH];
#ifdef PROFILER_COLLECT_PC
  u2 pcs[MAX_STACK_DEPTH];
#endif

  StackSlot() : state(StackSlotState::FREE), depth(0) {}
};

struct ProfileState {
  pthread_key_t threadIsProfilingKey;
  pid_t processId;
  int availableTracers;
  int currentTracers;
  std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracersMap;
  StackSlot stacks[MAX_STACKS_COUNT];
  std::atomic<uint32_t> currentSlot;
  // Error stats
  std::atomic<uint16_t> errSigCrashes;
  std::atomic<uint16_t> errSlotMisses;
  std::atomic<uint16_t> errStackOverflows;

  int64_t profileStartTime;
  std::atomic<uint32_t> fullSlotsCounter;
  sem_t slotsCounterSem;
  std::atomic_bool isLoggerLoopDone;

  bool wallClockModeEnabled;
  int samplingRateUs;
  std::atomic_bool enoughStacks;

  // When in "wall clock mode", we can optionally whitelist additional threads
  // to profile as well.
  std::unordered_set<int32_t> whitelistedThreads;
  // Guards whitelistedThreads.
  std::mutex whitelistMtx;

  // If a secondary trace starts, we need to tell the logger loop to clear
  // its cache of logged frames, so that the new trace won't miss any symbols
  std::atomic_bool resetFrameworkSymbols;

  ProfileState() {
    int ret;
    if ((ret = pthread_key_create(&threadIsProfilingKey, nullptr))) {
      throw std::system_error(
          ret, std::system_category(), "pthread_key_create");
    }
  }
};

/**
 * Exported functions
 */

class SamplingProfiler {
 public:
  static bool initialize(fbjni::alias_ref<jobject> ref, jint available_tracers);

  static void loggerLoop(fbjni::alias_ref<jobject> obj);

  static void stopProfiling(fbjni::alias_ref<jobject> obj);

  static bool startProfiling(
      fbjni::alias_ref<jobject> obj,
      int requested_providers,
      int sampling_rate_ms,
      bool wall_clock_mode_enabled);

  static void addToWhitelist(fbjni::alias_ref<jobject> obj, int targetThread);

  static void removeFromWhitelist(
      fbjni::alias_ref<jobject> obj,
      int targetThread);

  static void resetFrameworkNamesSet(fbjni::alias_ref<jobject> obj);

 private:
  static ProfileState& getProfileState();

  static void initSignalHandlers();

  static void maybeSignalReader();

  static void flushStackTraces(std::unordered_set<uint64_t>& loggedFramesSet);

  static sigmux_action FaultHandler(sigmux_siginfo*, void*);

  static sigmux_action UnwindStackHandler(sigmux_siginfo*, void*);
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
