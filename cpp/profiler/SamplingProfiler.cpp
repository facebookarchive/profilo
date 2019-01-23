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

#include "SamplingProfiler.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <random>
#include <string>

#include <fb/Build.h>
#include <fb/log.h>
#include <fbjni/fbjni.h>
#include <sigmux.h>

#include "DalvikTracer.h"
#include "profiler/ArtUnwindcTracer_600.h"
#include "profiler/ArtUnwindcTracer_700.h"
#include "profiler/ArtUnwindcTracer_710.h"
#include "profiler/ArtUnwindcTracer_711.h"
#include "profiler/ArtUnwindcTracer_712.h"
#include "profiler/ExternalTracerManager.h"
#include "profiler/JSTracer.h"
#include "profiler/JavaBaseTracer.h"

#if HAS_NATIVE_TRACER
#include <profiler/NativeTracer.h>
#endif

#include "profilo/LogEntry.h"
#include "profilo/Logger.h"
#include "profilo/TraceProviders.h"

#include "util/common.h"

using namespace facebook::jni;
using namespace facebook::profilo::util;

namespace facebook {
namespace profilo {
namespace profiler {

namespace {

// Function wrapper around the static profile state to avoid
// using a DSO constructor.
ProfileState& getProfileState() {
  //
  // Despite the fact that this is accessed from a signal handler (this routine
  // is not async-signal safe due to the initialization lock for this variable),
  // this is safe. The first access will always be before the first access from
  // a signal context, so the variable is guaranteed to be initialized by then.
  //
  static ProfileState state;
  return state;
}

static void throw_errno(const char* error) {
  throw std::system_error(errno, std::system_category(), error);
}

static bool threadIsUnwinding(ProfileState const& profileState) {
  return pthread_getspecific(profileState.threadIsProfilingKey) != nullptr;
}

sigmux_action sigcatch_handler(
    struct sigmux_siginfo* siginfo,
    void* handler_data) {
  ProfileState& profileState = *reinterpret_cast<ProfileState*>(handler_data);
  if (!threadIsUnwinding(profileState)) {
    return SIGMUX_CONTINUE_SEARCH;
  }

  int32_t tid = threadID();

  // We know the thread that raised the signal was unwinding, find its slot and
  // jump to the saved state there.
  for (int i = 0; i < MAX_STACKS_COUNT; i++) {
    auto& slot = profileState.stacks[i];

    uint32_t targetBusyState = (tid << 16) | StackSlotState::BUSY;
    // If slot matches free it, increase error count and jump out
    if (slot.state.compare_exchange_strong(
            targetBusyState, StackSlotState::FREE)) {
      profileState.errSigCrashes.fetch_add(1);
      sigmux_longjmp(siginfo, slot.sig_jmp_buf, 1);
    }
  }
  // Something is wrong. This thread is supposed to be unwinding but we
  // couldn't find its slot. Something is corrupted in our state, abort.
  abort();
}

void maybeSignalReader(bool stackCollected) {
  if (!stackCollected) {
    return;
  }

  auto& profileState = getProfileState();
  uint32_t prevSlotCounter = profileState.fullSlotsCounter.fetch_add(1);
  if ((prevSlotCounter + 1) % FLUSH_STACKS_COUNT == 0) {
    if (profileState.wallClockModeEnabled) {
      profileState.enoughStacks.store(true);
    } else {
      int res = sem_post(&profileState.slotsCounterSem);
      if (res != 0) {
        abort(); // Something went wrong
      }
    }
  }
}

sigmux_action sigprof_handler(
    struct sigmux_siginfo* siginfo,
    void* handler_data) {
  ProfileState& profileState = *reinterpret_cast<ProfileState*>(handler_data);

  if (threadIsUnwinding(profileState)) {
    // Jump out, we're already in this handler.
    return SIGMUX_CONTINUE_EXECUTION;
  }

  pthread_setspecific(profileState.threadIsProfilingKey, (void*)1);

  auto tid = threadID();
  for (const auto& tracerEntry : profileState.tracersMap) {
    auto tracerType = tracerEntry.first;
    if (!(tracerType & profileState.currentTracers)) {
      continue;
    }
    auto slotIndex = profileState.currentSlot.fetch_add(1);
    bool slot_found = false;
    // Busy state includes thread id to avoid race condition on slot's tid.
    uint32_t busyState = StackSlotState::BUSY | (tid << 16);

    for (int i = 0; i < MAX_STACKS_COUNT; i++) {
      auto nextSlotIndex = (slotIndex + i) % MAX_STACKS_COUNT;
      auto& slot = profileState.stacks[nextSlotIndex];

      // Verify if the slot is actually in FREE state and switch it
      // to BUSY atomically. If slot is not FREE then just exit.
      // Normally free slot will be available from the first iteration.
      uint32_t expected = StackSlotState::FREE;
      if (slot.state.compare_exchange_strong(expected, busyState)) {
        slotIndex = nextSlotIndex;
        slot_found = true;
        break;
      }
    }

    if (!slot_found) {
      profileState.errSlotMisses.fetch_add(1);
      // We're out of slots, no tracer is likely to succeed.
      break;
    }

    auto& slot = profileState.stacks[slotIndex];

    // Can finally occupy the slot
    if (sigsetjmp(slot.sig_jmp_buf, 1) == 0) {
      memset(slot.method_names, 0, sizeof(slot.method_names));
      memset(slot.class_descriptors, 0, sizeof(slot.class_descriptors));
      bool success{};
      if (JavaBaseTracer::isJavaTracer(tracerType)) {
        success = reinterpret_cast<JavaBaseTracer*>(tracerEntry.second.get())
                      ->collectJavaStack(
                          (ucontext_t*)siginfo->context,
                          slot.frames,
                          slot.method_names,
                          slot.class_descriptors,
                          slot.depth,
                          (uint8_t)MAX_STACK_DEPTH);
      } else {
        success = tracerEntry.second->collectStack(
            (ucontext_t*)siginfo->context,
            slot.frames,
            slot.depth,
            (uint8_t)MAX_STACK_DEPTH);
      }

      if (success) {
        slot.time = monotonicTime();
        slot.profilerType = tracerType;
      } else {
        profileState.errStackOverflows.fetch_add(1);
      }

      if (!slot.state.compare_exchange_strong(
              busyState,
              success ? ((tid << 16) | StackSlotState::FULL)
                      : StackSlotState::FREE)) {
        // Slot was overwritten by another thread.
        // This is an ordering violation, so abort.
        abort();
      }

      maybeSignalReader(success);
      continue;
    } else {
      // We came from the longjmp in sigcatch_handler.
      // Something must have crashed.
      // Don't continue if any tracer fails.
      break;
    }
  }

  pthread_setspecific(profileState.threadIsProfilingKey, (void*)0);
  return SIGMUX_CONTINUE_EXECUTION;
}

void initSignalHandlers() {
  //
  // Register a handler for SIGPROF, trusting that sigmux will internally use
  // SA_RESTART.
  //
  // Also, register a handler for SIGSEGV and SIGBUS, so that we can safely
  // jump away in the case of a crash in our SIGPROF handler.
  //

  // Signal to be be handled when collecting stack traces
  static constexpr const int kAccessSignals[] = {SIGSEGV, SIGBUS};
  static constexpr auto kNumAccessSignals =
      sizeof(kAccessSignals) / sizeof(*kAccessSignals);

  auto& state = getProfileState();

  if (sigmux_init(SIGPROF)) {
    throw_errno("Couldn't init sigmux for SIGPROF");
  }

  sigset_t prof_set;
  if (sigemptyset(&prof_set)) {
    throw_errno("sigemptyset");
  }

  if (sigaddset(&prof_set, SIGPROF)) {
    throw_errno("sigaddset");
  }

  if (!sigmux_register(&prof_set, sigprof_handler, &state, 0)) {
    throw_errno("sigmux_register for SIGPROF");
  }

  // register handler to ignore potentially sigsegv/sigbus
  sigset_t sigset;
  if (sigemptyset(&sigset)) {
    throw_errno("Couldn't sigemptyset");
  }

  for (size_t i = 0; i < kNumAccessSignals; i++) {
    int signum = kAccessSignals[i];
    if (sigaddset(&sigset, signum)) {
      FBLOGE("Failed to add signal %d to sigset: %s", signum, strerror(errno));
      throw_errno("Couldn't add signal for SIGSEGV/SIGBUS");
    }

    if (sigmux_init(signum)) {
      FBLOGE(
          "Failed to init sigmux with signal %d: %s", signum, strerror(errno));
      throw_errno("Couldn't init sigmux for SIGSEGV/SIGBUS");
    }
  }

  if (!sigmux_register(&sigset, sigcatch_handler, &state, 0)) {
    FBLOGE("Failed to register sigmux: %s", strerror(errno));
    throw_errno("Couldn't register sigmux for SIGSEGV/SIGBUS signal jail");
  }
}

void flushStackTraces(std::unordered_set<uint64_t>& loggedFramesSet) {
  auto& profileState = getProfileState();

  int processedCount = 0;
  for (size_t i = 0; i < MAX_STACKS_COUNT; i++) {
    auto& slot = profileState.stacks[i];

    uint32_t slotStateCombo = slot.state.load();
    uint32_t slotState = slotStateCombo & 0xffff;
    if (StackSlotState::FULL != slotState) {
      continue;
    }

    // Ignore remains from a previous trace
    if (slot.time > profileState.profileStartTime) {
      auto& tracer = profileState.tracersMap[slot.profilerType];
      auto tid = slotStateCombo >> 16;
      tracer->flushStack(slot.frames, slot.depth, tid, slot.time);

      if (JavaBaseTracer::isJavaTracer(slot.profilerType)) {
        for (int i = 0; i < slot.depth; i++) {
          bool expectedResetState = true;
          if (profileState.resetFrameworkSymbols.compare_exchange_strong(
                  expectedResetState, false)) {
            loggedFramesSet.clear();
          }

          if (loggedFramesSet.find(slot.frames[i]) == loggedFramesSet.end() &&
              JavaBaseTracer::isFramework(slot.class_descriptors[i])) {
            StandardEntry entry{};
            entry.tid = threadID();
            entry.timestamp = monotonicTime();
            entry.type = entries::JAVA_FRAME_NAME;
            entry.extra = slot.frames[i];
            int32_t id = Logger::get().write(std::move(entry));

            std::string full_name{slot.class_descriptors[i]};
            full_name += slot.method_names[i];
            Logger::get().writeBytes(
                entries::STRING_VALUE,
                id,
                (const uint8_t*)full_name.c_str(),
                full_name.length());
          }
          // Mark the frame as "logged" or "visited" so that we don't do a
          // string comparison for it next time, regardless of whether it was
          // a framework frame or not
          loggedFramesSet.insert(slot.frames[i]);
        }
      }
    }

    uint32_t expected = slotStateCombo;
    // Release the slot
    if (!slot.state.compare_exchange_strong(expected, StackSlotState::FREE)) {
      // Slot was re-used in the middle of the processing by another thread.
      // Aborting.
      abort();
    }
    processedCount++;
  }
  FBLOGV("Stacks flush is done. Processed %d stacks", processedCount);
}

void logProfilingErrAnnotation(int32_t key, uint16_t value) {
  if (value == 0) {
    return;
  }
  Logger::get().writeTraceAnnotation(key, value);
}
} // namespace

/**
 * Initializes the profiler. Registers handler for custom defined SIGPROF
 * symbol which will collect traces and inits thread/process ids
 */
bool initialize(alias_ref<jobject> ref, jint available_tracers) {
  auto& profileState = getProfileState();

  profileState.processId = getpid();
  profileState.availableTracers = available_tracers;

  if (available_tracers & tracers::DALVIK) {
    profileState.tracersMap[tracers::DALVIK] = std::make_shared<DalvikTracer>();
  }

#if HAS_NATIVE_TRACER && __arm__
  if (available_tracers & tracers::NATIVE) {
    profileState.tracersMap[tracers::NATIVE] = std::make_shared<NativeTracer>();
  }
#endif

  if (available_tracers & tracers::ART_UNWINDC_6_0) {
    profileState.tracersMap[tracers::ART_UNWINDC_6_0] =
        std::make_shared<ArtUnwindcTracer60>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_0_0) {
    profileState.tracersMap[tracers::ART_UNWINDC_7_0_0] =
        std::make_shared<ArtUnwindcTracer700>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_1_0) {
    profileState.tracersMap[tracers::ART_UNWINDC_7_1_0] =
        std::make_shared<ArtUnwindcTracer710>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_1_1) {
    profileState.tracersMap[tracers::ART_UNWINDC_7_1_1] =
        std::make_shared<ArtUnwindcTracer711>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_1_2) {
    profileState.tracersMap[tracers::ART_UNWINDC_7_1_2] =
        std::make_shared<ArtUnwindcTracer712>();
  }

  if (available_tracers & tracers::JAVASCRIPT) {
    auto jsTracer = std::make_shared<JSTracer>();
    profileState.tracersMap[tracers::JAVASCRIPT] = jsTracer;
    ExternalTracerManager::getInstance().registerExternalTracer(jsTracer);
  }

  initSignalHandlers();

  // Init semaphore for stacks flush to the Ring Buffer
  int res = sem_init(&profileState.slotsCounterSem, 0, 0);
  if (res != 0) {
    FBLOGV("Can not init semaphore: %s", strerror(errno));
    errno = 0;
    return false;
  }

  return true;
}

/**
 * Called via JNI from CPUProfiler
 *
 * Waits in a loop for semaphore wakeup and then flushes the current profiling
 * stacks.
 */
void loggerLoop(alias_ref<jobject> obj) {
  FBLOGV("Logger thread is going into the loop...");
  auto& profileState = getProfileState();

  profileState.isLoggerLoopDone.store(false);
  profileState.enoughStacks.store(false);

  // If we are targeting a subset of all the threads, then the setitimer logic
  // is not used; instead, we make this thread sleep the right amount of time
  // and, when it wakes up, we send a tgkill(SIGPROF) to the interested threads

  int res;
  bool done;
  std::unordered_set<uint64_t> loggedFramesSet{};

  do {
    if (profileState.wallClockModeEnabled) {
      usleep(profileState.samplingRateUs);
      std::unique_lock<std::mutex> lock(profileState.whitelistMtx);
      for (int32_t tid : profileState.whitelistedThreads) {
        res = syscall(__NR_tgkill, profileState.processId, tid, SIGPROF);
        if (res != 0 && errno == ESRCH) {
          // If the thread id is bad or no longer exists, remove it from our
          // whitelist.
          profileState.whitelistedThreads.erase(tid);
        }
        if (res == 0 && profileState.enoughStacks.load()) {
          flushStackTraces(loggedFramesSet);
          profileState.enoughStacks.store(false);
        }
      }
    } else {
      // setitimer (i.e. every thread) case
      res = sem_wait(&profileState.slotsCounterSem);
      if (res == 0) {
        flushStackTraces(loggedFramesSet);
      }
    }
    done = profileState.isLoggerLoopDone.load();
  } while (!done && (res == 0 || errno == EINTR));

  FBLOGV("Logger thread is shutting down...");
}

bool startProfiling(
    fbjni::alias_ref<jobject> obj,
    int requested_tracers,
    int sampling_rate_ms,
    bool wall_clock_mode_enabled) {
  FBLOGV("Start profiling");
  auto& profileState = getProfileState();

  profileState.profileStartTime = monotonicTime();
  profileState.currentTracers =
      profileState.availableTracers & requested_tracers;

  if (profileState.currentTracers == 0) {
    return false;
  }

  profileState.samplingRateUs = sampling_rate_ms * 1000;
  profileState.wallClockModeEnabled = wall_clock_mode_enabled;

  for (const auto& tracerEntry : profileState.tracersMap) {
    if (tracerEntry.first & profileState.currentTracers) {
      tracerEntry.second->startTracing();
    }
  }

  // Call setitimer only if not in "single thread sampling" mode
  if (!profileState.wallClockModeEnabled) {
    auto sampleRateMicros = sampling_rate_ms * 1000;
    // Generate random initial delay. Used to calculate the initial trace delay
    // to avoid sampling bias.
    std::mt19937 randGenerator(std::time(0));
    std::uniform_int_distribution<> randDistribution(1, sampleRateMicros);
    auto sampleStartDelayMicros = randDistribution(randGenerator);

    itimerval tv;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = sampleStartDelayMicros;
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = sampleRateMicros;

    int res = setitimer(ITIMER_PROF, &tv, nullptr);
    if (res != 0) {
      FBLOGV("Can not start itimer: %s", strerror(errno));
      errno = 0;
      return false;
    }
  }

  FBLOGV("Current tracers mask: %d", profileState.currentTracers);
  return true;
}

/**
 * Stop the profiler. Write collected stack traces to profilo
 * The value to write will be a 64 bit <method_id, dex_number>.
 * Unfortunately, DvmDex or DvmHeader doesn't contain a unique dex number that
 * we could reuse. Until this is possibly written custom by redex, we'll use
 * checksum for the dex identification which should collide rare.
 */
void stopProfiling(fbjni::alias_ref<jobject> obj) {
  auto& profileState = getProfileState();

  FBLOGV("Stopping profiling");

  if (!profileState.wallClockModeEnabled) {
    itimerval tv;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 0;
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = 0;

    // Stop the timer
    int res = setitimer(ITIMER_PROF, &tv, nullptr);
    if (res != 0) {
      FBLOGV("Cannot stop itimer: %s", strerror(errno));
      abort();
    }

    profileState.isLoggerLoopDone.store(true);
    res = sem_post(&profileState.slotsCounterSem);
    if (res != 0) {
      FBLOGV("Can not execute sem_post for logger thread");
      errno = 0;
    }
  } else {
    profileState.isLoggerLoopDone.store(true);
  }

  // Logging errors
  logProfilingErrAnnotation(
      QuickLogConstants::PROF_ERR_SIG_CRASHES, profileState.errSigCrashes);
  logProfilingErrAnnotation(
      QuickLogConstants::PROF_ERR_SLOT_MISSES, profileState.errSlotMisses);
  logProfilingErrAnnotation(
      QuickLogConstants::PROF_ERR_STACK_OVERFLOWS,
      profileState.errStackOverflows);

  FBLOGV(
      "Stack overflows = %d, Sig crashes = %d, Slot misses = %d",
      profileState.errStackOverflows.load(),
      profileState.errSigCrashes.load(),
      profileState.errSlotMisses.load());

  profileState.currentSlot = 0;
  profileState.errSigCrashes = 0;
  profileState.errSlotMisses = 0;
  profileState.errStackOverflows = 0;

  for (const auto& tracerEntry : profileState.tracersMap) {
    if (tracerEntry.first & profileState.currentTracers) {
      tracerEntry.second->stopTracing();
    }
  }
}

void addToWhitelist(fbjni::alias_ref<jobject> obj, int targetThread) {
  auto& profileState = getProfileState();
  std::unique_lock<std::mutex> lock(profileState.whitelistMtx);
  profileState.whitelistedThreads.insert(static_cast<int32_t>(targetThread));
}

void removeFromWhitelist(fbjni::alias_ref<jobject> obj, int targetThread) {
  auto& profileState = getProfileState();
  std::unique_lock<std::mutex> lock(profileState.whitelistMtx);
  profileState.whitelistedThreads.erase(targetThread);
}

void resetFrameworkNamesSet(fbjni::alias_ref<jobject> obj) {
  auto& profileState = getProfileState();

  // Let the logger loop know we should reset our cache of frames
  profileState.resetFrameworkSymbols.store(true);
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
