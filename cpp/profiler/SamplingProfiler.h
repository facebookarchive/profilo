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
#include <signal.h>
#include <unordered_map>

#if !defined(__GLIBCXX__)
#include <__threading_support>
#endif

#include <fbjni/fbjni.h>

#include <profiler/Constants.h>

#include "DalvikTracer.h"
#include "profilo/LogEntry.h"
#include "util/ProcFs.h"


namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace profiler {

enum StackSlotState: uint8_t {
  FREE = 1,
  BUSY = 2,
  FULL = 3,
};

struct StackSlot {
  std::atomic<uint32_t> state;
  uint8_t depth;
  int64_t time;
  jmp_buf sig_jmp_buf;
  uint32_t profilerType;
  int64_t frames[MAX_STACK_DEPTH]; // frame pointer addresses
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
  std::unordered_map<int32_t, std::unique_ptr<BaseTracer>> tracersMap;
  StackSlot stacks[MAX_STACKS_COUNT];
  std::atomic<uint32_t> currentSlot;
  // Error stats
  std::atomic<uint16_t> errSigCrashes;
  std::atomic<uint16_t> errSlotMisses;
  std::atomic<uint16_t> errStackOverflows;

  struct sigaction oldSigprofAct;

  int64_t profileStartTime;
  std::atomic<uint32_t> fullSlotsCounter;
  sem_t slotsCounterSem;
  std::atomic_bool isLoggerLoopDone;

  // Allow targeting individual thread
  int32_t targetThread;
  bool singleThreadMode;
  int samplingRateUs;
  std::atomic_bool enoughStacks;

  ProfileState() {
    int ret;
    if ((ret = pthread_key_create(&threadIsProfilingKey, nullptr))) {
      throw std::system_error(ret, std::system_category(), "pthread_key_create");
    }
  }
};

/**
 * Exported functions
 */
bool initialize(fbjni::alias_ref<jobject> ref, jint available_tracers);
void loggerLoop(fbjni::alias_ref<jobject> obj);
void stopProfiling(fbjni::alias_ref<jobject> obj);
bool startProfiling(
  fbjni::alias_ref<jobject> obj,
  int requested_providers,
  int sampling_rate_ms,
  int targetThread);

} // namespace profiler
} // namespace profilo
} // namespace facebook
