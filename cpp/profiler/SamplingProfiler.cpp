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
#include "TimerManager.h"

#include <abort_with_reason.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <chrono>
#include <random>
#include <string>

#include <fb/log.h>
#include <fbjni/fbjni.h>

#include <profiler/ArtUnwindcTracer_500.h>
#include <profiler/ArtUnwindcTracer_510.h>
#include <profiler/ArtUnwindcTracer_600.h>
#include <profiler/ArtUnwindcTracer_700.h>
#include <profiler/ArtUnwindcTracer_710.h>
#include <profiler/ArtUnwindcTracer_711.h>
#include <profiler/ArtUnwindcTracer_712.h>
#include <profiler/ArtUnwindcTracer_800.h>
#include <profiler/ArtUnwindcTracer_810.h>
#include <profiler/DalvikTracer.h>
#include <profiler/ExternalTracerManager.h>
#include <profiler/JSTracer.h>
#include <profiler/JavaBaseTracer.h>
#include <profilo/ExternalApi.h>

#if HAS_NATIVE_TRACER
#include <profiler/NativeTracer.h>
#endif

#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <profilo/TraceProviders.h>

#include <util/ProcFs.h>
#include <util/common.h>

using namespace facebook::jni;

namespace facebook {
namespace profilo {
namespace profiler {

namespace {
constexpr auto kMicrosecondsInMillisecond = 1000;

EntryType errorToTraceEntry(StackCollectionRetcode error) {
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wswitch"

  switch (error) {
    case StackCollectionRetcode::EMPTY_STACK:
      return entries::STKERR_EMPTYSTACK;

    case StackCollectionRetcode::STACK_OVERFLOW:
      return entries::STKERR_STACKOVERFLOW;

    case StackCollectionRetcode::NO_STACK_FOR_THREAD:
      return entries::STKERR_NOSTACKFORTHREAD;

    case StackCollectionRetcode::SIGNAL_INTERRUPT:
      return entries::STKERR_SIGNALINTERRUPT;

    case StackCollectionRetcode::NESTED_UNWIND:
      return entries::STKERR_NESTEDUNWIND;

    case StackCollectionRetcode::TRACER_DISABLED:
    case StackCollectionRetcode::SUCCESS:
    case StackCollectionRetcode::MAXVAL:
      return entries::UNKNOWN_TYPE;
  }

#pragma clang diagnostic pop
}

static void throw_errno(const char* error) {
  throw std::system_error(errno, std::system_category(), error);
}

// --- setitimer wrappers
bool startSetitimer(int sampling_rate_ms) {
  itimerval tv = getInitialItimerval(sampling_rate_ms);
  int res = setitimer(ITIMER_PROF, &tv, nullptr);
  if (res != 0) {
    FBLOGV("Can not start itimer: %s", strerror(errno));
    errno = 0;
    return false;
  }
  return true;
}

bool stopSetitimer() {
  itimerval tv;
  tv.it_value.tv_sec = 0;
  tv.it_value.tv_usec = 0;
  tv.it_interval.tv_sec = 0;
  tv.it_interval.tv_usec = 0;
  int res = setitimer(ITIMER_PROF, &tv, nullptr);
  if (res != 0) {
    FBLOGV("Cannot stop itimer: %s", strerror(errno));
    return false;
  }
  return true;
}

} // namespace

SamplingProfiler& SamplingProfiler::getInstance() {
  //
  // Despite the fact that this is accessed from a signal handler (this routine
  // is not async-signal safe due to the initialization lock for this variable),
  // this is safe. The first access will always be before the first access from
  // a signal context, so the variable is guaranteed to be initialized by then.
  //
  static SamplingProfiler profiler;
  return profiler;
}

sigmux_action SamplingProfiler::FaultHandler(
    struct sigmux_siginfo* siginfo,
    void* handler_data) {
  ProfileState& state =
      reinterpret_cast<SamplingProfiler*>(handler_data)->state_;

  int32_t tid = threadID();
  uint32_t targetBusyState = (tid << 16) | StackSlotState::BUSY_WITH_METADATA;

  // Find the most recent slot occupied by this thread.
  // This allows us to handle crashes during nested unwinding from
  // the most inner one out.
  int64_t max_time = -1;
  int max_idx = -1;

  for (int i = 0; i < MAX_STACKS_COUNT; i++) {
    auto& slot = state.stacks[i];
    if (slot.state.load() == targetBusyState && slot.time > max_time) {
      max_time = slot.time;
      max_idx = i;
    }
  }

  if (max_idx >= 0) {
    state.errSigCrashes.fetch_add(1);
    sigmux_longjmp(siginfo, state.stacks[max_idx].sig_jmp_buf, 1);
  }

  return SIGMUX_CONTINUE_SEARCH;
}

void SamplingProfiler::maybeSignalReader() {
  uint32_t prevSlotCounter = state_.fullSlotsCounter.fetch_add(1);
  if ((prevSlotCounter + 1) % FLUSH_STACKS_COUNT == 0) {
    if (state_.useSleepBasedWallProfiler) {
      state_.enoughStacks.store(true);
    } else {
      int res = sem_post(&state_.slotsCounterSem);
      if (res != 0) {
        abort(); // Something went wrong
      }
    }
  }
}

// Finds the next FREE slot and atomically sets its state to BUSY, so that
// the acquiring thread can safely write to it, and returns the index via
// <outSlot>. Returns true if a FREE slot was found, false otherwise.
bool getSlotIndex(ProfileState& state_, uint32_t tid, uint32_t& outSlot) {
  auto slotIndex = state_.currentSlot.fetch_add(1);
  for (int i = 0; i < MAX_STACKS_COUNT; i++) {
    auto nextSlotIndex = (slotIndex + i) % MAX_STACKS_COUNT;
    auto& slot = state_.stacks[nextSlotIndex];
    uint32_t expected = StackSlotState::FREE;
    uint32_t targetBusyState = (tid << 16) | StackSlotState::BUSY;
    if (slot.state.compare_exchange_strong(expected, targetBusyState)) {
      outSlot = nextSlotIndex;

      slot.time = monotonicTime();
      memset(&slot.sig_jmp_buf, 0, sizeof(slot.sig_jmp_buf));

      expected = targetBusyState;
      targetBusyState = (tid << 16) | StackSlotState::BUSY_WITH_METADATA;
      if (!slot.state.compare_exchange_strong(expected, targetBusyState)) {
        abortWithReason(
            "Invariant violation - BUSY to BUSY_WITH_METADATA failed");
      }
      return true;
    }
  }
  // We didn't find an empty slot, so bump our counter
  state_.errSlotMisses.fetch_add(1);
  return false;
}

sigmux_action SamplingProfiler::UnwindStackHandler(
    struct sigmux_siginfo* siginfo,
    void* handler_data) {
  auto profiler = reinterpret_cast<SamplingProfiler*>(handler_data);
  auto& state = profiler->state_;

  auto tid = threadID();
  uint32_t busyState = (tid << 16) | StackSlotState::BUSY_WITH_METADATA;

  for (const auto& tracerEntry : state.tracersMap) {
    auto tracerType = tracerEntry.first;
    if (!(tracerType & state.currentTracers)) {
      continue;
    }

    // The external tracer is frequently disabled, so fail fast here
    // if that is the case
    if (ExternalTracer::isExternalTracer(tracerType)) {
      if (!static_cast<ExternalTracer*>(tracerEntry.second.get())
               ->isEnabled()) {
        continue;
      }
    }

    uint32_t slotIndex;
    bool slot_found = getSlotIndex(state, tid, slotIndex);
    if (!slot_found) {
      // We're out of slots, no tracer is likely to succeed.
      break;
    }

    auto& slot = state.stacks[slotIndex];

    // Can finally occupy the slot
    if (sigsetjmp(slot.sig_jmp_buf, 1) == 0) {
      memset(slot.method_names, 0, sizeof(slot.method_names));
      memset(slot.class_descriptors, 0, sizeof(slot.class_descriptors));
      uint8_t ret{StackSlotState::FREE};
      if (JavaBaseTracer::isJavaTracer(tracerType)) {
        ret = reinterpret_cast<JavaBaseTracer*>(tracerEntry.second.get())
                  ->collectJavaStack(
                      (ucontext_t*)siginfo->context,
                      slot.frames,
                      slot.method_names,
                      slot.class_descriptors,
                      slot.depth,
                      (uint8_t)MAX_STACK_DEPTH);
      } else {
        ret = tracerEntry.second->collectStack(
            (ucontext_t*)siginfo->context,
            slot.frames,
            slot.depth,
            (uint8_t)MAX_STACK_DEPTH);
      }

      slot.profilerType = tracerType;
      if (StackCollectionRetcode::STACK_OVERFLOW == ret) {
        state.errStackOverflows.fetch_add(1);
      }

      // Ignore TRACER_DISABLED errors for now and free the slot.
      // TODO T42938550
      if (StackCollectionRetcode::TRACER_DISABLED == ret) {
        if (!slot.state.compare_exchange_strong(
                busyState, StackSlotState::FREE)) {
          abortWithReason(
              "Invariant violation - BUSY_WITH_METADATA to FREE failed");
        }
        continue;
      }

      if (!slot.state.compare_exchange_strong(busyState, (tid << 16) | ret)) {
        // Slot was overwritten by another thread.
        // This is an ordering violation, so abort.
        abortWithReason(
            "Invariant violation - BUSY_WITH_METADATA to return code failed");
      }

      profiler->maybeSignalReader();
      continue;
    } else {
      // We came from the longjmp in sigcatch_handler.
      // Something must have crashed.
      // Log the error information and bail out
      slot.time = monotonicTime();
      slot.profilerType = tracerType;
      if (!slot.state.compare_exchange_strong(
              busyState,
              (tid << 16) | StackCollectionRetcode::SIGNAL_INTERRUPT)) {
        abortWithReason(
            "Invariant violation - BUSY_WITH_METADATA to SIGNAL_INTERRUPT failed");
      }
      break;
    }
  }

  return SIGMUX_CONTINUE_EXECUTION;
}

void SamplingProfiler::registerSignalHandlers() {
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

  sigmux_state.profiling_registration =
      sigmux_register(&prof_set, UnwindStackHandler, this, 0);
  if (!sigmux_state.profiling_registration) {
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

  sigmux_state.fault_registration =
      sigmux_register(&sigset, FaultHandler, this, 0);
  if (!sigmux_state.fault_registration) {
    sigmux_unregister(sigmux_state.profiling_registration);
    sigmux_state.profiling_registration = nullptr;

    FBLOGE("Failed to register sigmux: %s", strerror(errno));
    throw_errno("Couldn't register sigmux for SIGSEGV/SIGBUS signal jail");
  }
}

void SamplingProfiler::unregisterSignalHandlers() {
  // There are multiple cases we need to worry about:
  //   a) currently executing profiling handlers
  //   b) pending profiling signals
  //   c) currently executing fault handlers
  //   d) pending fault signals
  //
  // Observe that fault handlers return to the profiling handler and
  // are conceptually nested within them.
  //   PROF_ENTER
  //     FAULT_ENTER
  //     FAULT_LONGJMP
  //   PROF_EXIT
  //
  // By waiting for all profiling handlers to finish (which sigmux_unregister
  // does internally), we solve a), c), and d) (pending fault signals during a
  // profiling signal means we won't exit the corresponding profiling handler
  // until we've handled the fault).
  //
  // To solve b), we change the signal disposition for SIGPROF from SIG_DFL to
  // SIG_IGN. We only do this if the original handler before sigmux seized it
  // was SIG_DFL.
  //

  struct sigaction original_sigaction;
  if (sigmux_sigaction(SIGPROF, nullptr, &original_sigaction)) {
    throw_errno("Could not read original sigaction for profiling signal");
  }

  if (original_sigaction.sa_handler == SIG_DFL) {
    FBLOGV("Replacing default disposition for profiling signal with IGN");
    struct sigaction ign_sigaction {
      .sa_handler = SIG_IGN,
    };
    if (sigmux_sigaction(SIGPROF, &ign_sigaction, nullptr)) {
      throw_errno("Could not change disposition for profiling signal to IGN");
    }
  }

  if (sigmux_state.profiling_registration) {
    sigmux_unregister(sigmux_state.profiling_registration);
    sigmux_state.profiling_registration = nullptr;
  }

  if (sigmux_state.fault_registration) {
    sigmux_unregister(sigmux_state.fault_registration);
    sigmux_state.fault_registration = nullptr;
  }
}

void SamplingProfiler::flushStackTraces(
    std::unordered_set<uint64_t>& loggedFramesSet) {
  int processedCount = 0;
  for (size_t i = 0; i < MAX_STACKS_COUNT; i++) {
    auto& slot = state_.stacks[i];

    uint32_t slotStateCombo = slot.state.load();
    uint32_t slotState = slotStateCombo & 0xffff;
    if (slotState == StackSlotState::FREE ||
        slotState == StackSlotState::BUSY ||
        slotState == StackSlotState::BUSY_WITH_METADATA) {
      continue;
    }

    // Ignore remains from a previous trace
    if (slot.time > state_.profileStartTime) {
      auto& tracer = state_.tracersMap[slot.profilerType];
      auto tid = slotStateCombo >> 16;

      if (StackCollectionRetcode::SUCCESS == slotState) {
        tracer->flushStack(slot.frames, slot.depth, tid, slot.time);
      } else {
        StandardEntry entry{};
        entry.type = static_cast<EntryType>(
            errorToTraceEntry(static_cast<StackCollectionRetcode>(slotState)));
        entry.timestamp = slot.time;
        entry.tid = tid;
        entry.extra = slot.profilerType;
        Logger::get().write(std::move(entry));
      }

      if (JavaBaseTracer::isJavaTracer(slot.profilerType)) {
        for (int i = 0; i < slot.depth; i++) {
          bool expectedResetState = true;
          if (state_.resetFrameworkSymbols.compare_exchange_strong(
                  expectedResetState, false)) {
            loggedFramesSet.clear();
          }

          if (loggedFramesSet.find(slot.frames[i]) == loggedFramesSet.end() &&
              JavaBaseTracer::isFramework(slot.class_descriptors[i])) {
            StandardEntry entry{};
            entry.tid = tid;
            entry.timestamp = slot.time;
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
}

void logProfilingErrAnnotation(int32_t key, uint16_t value) {
  if (value == 0) {
    return;
  }
  Logger::get().writeTraceAnnotation(key, value);
}

/**
 * Initializes the profiler. Registers handler for custom defined SIGPROF
 * symbol which will collect traces and inits thread/process ids
 */
bool SamplingProfiler::initialize(
    int32_t available_tracers,
    std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracers) {
  state_.processId = getpid();
  state_.availableTracers = available_tracers;
  state_.tracersMap = std::move(tracers);
  state_.timerManager.reset();

  // Init semaphore for stacks flush to the Ring Buffer
  int res = sem_init(&state_.slotsCounterSem, 0, 0);
  if (res != 0) {
    FBLOGV("Can not init slotsCounterSem semaphore: %s", strerror(errno));
    errno = 0;
    return false;
  }

  return true;
}

/**
 * Called via JNI from CPUProfiler
 *
 * Must only be called if SamplingProfiler::startProfiling() returns true.
 *
 * Waits in a loop for semaphore wakeup and then flushes the current profiling
 * stacks.
 */
void SamplingProfiler::loggerLoop() {
  FBLOGV("Logger thread %d is going into the loop...", threadID());

  // If we are targeting a subset of all the threads, then the setitimer logic
  // is not used; instead, we make this thread sleep the right amount of time
  // and, when it wakes up, we send a tgkill(SIGPROF) to the interested threads

  int res = 0;
  std::unordered_set<uint64_t> loggedFramesSet{};

  do {
    if (state_.useSleepBasedWallProfiler) {
      // sleep-based-wall-profiling mode
      usleep(state_.samplingRateMs * kMicrosecondsInMillisecond);
      std::unique_lock<std::mutex> lock(
          state_.whitelist->whitelistedThreadsMtx);
      for (int32_t tid : state_.whitelist->whitelistedThreads) {
        res = syscall(__NR_tgkill, state_.processId, tid, SIGPROF);
        if (res != 0 && errno == ESRCH) {
          // If the thread id is bad or no longer exists, remove it from our
          // whitelist.
          state_.whitelist->whitelistedThreads.erase(tid);
        }
        if (res == 0 && state_.enoughStacks.load()) {
          flushStackTraces(loggedFramesSet);
          state_.enoughStacks.store(false);
        }
      }
    } else {
      // setitimer and thread-specific timers
      res = sem_wait(&state_.slotsCounterSem);
      if (res == 0) {
        flushStackTraces(loggedFramesSet);
      }
    }
  } while (!state_.isLoggerLoopDone && (res == 0 || errno == EINTR));
  FBLOGV("Logger thread is shutting down...");
}

bool SamplingProfiler::startProfilingTimers() {
  FBLOGI("Starting profiling timers w/sample rate %d", state_.samplingRateMs);
  if (!state_.useThreadSpecificProfiler) {
    return startSetitimer(state_.samplingRateMs); // use global CPU timer
  } else { // thread-specific timers
    state_.timerManager.reset(new TimerManager(
        state_.threadDetectIntervalMs,
        state_.samplingRateMs,
        state_.wallClockModeEnabled,
        state_.wallClockModeEnabled ? state_.whitelist : nullptr));
    state_.timerManager->start();
  }
  return true;
}

bool SamplingProfiler::stopProfilingTimers() {
  if (!state_.useThreadSpecificProfiler) {
    return stopSetitimer(); // use global CPU timer
  } else { // thread-specific timers
    state_.timerManager->stop();
    state_.timerManager.reset();
  }
  return true;
}

bool SamplingProfiler::startProfiling(
    int requested_tracers,
    int sampling_rate_ms,
    bool use_thread_specific_profiler,
    int thread_detect_interval_ms,
    bool wall_clock_mode_enabled) {
  if (state_.isProfiling) {
    throw std::logic_error("startProfiling called while already profiling");
  }
  state_.isProfiling = true;
  FBLOGV("Start profiling");

  registerSignalHandlers();

  state_.profileStartTime = monotonicTime();
  state_.currentTracers = state_.availableTracers & requested_tracers;

  if (state_.currentTracers == 0) {
    return false;
  }

  constexpr auto kMinThreadDetectIntervalMs = 13; // TODO_YM T63620953
  if (thread_detect_interval_ms < kMinThreadDetectIntervalMs) {
    thread_detect_interval_ms = kMinThreadDetectIntervalMs;
  }

  state_.samplingRateMs = sampling_rate_ms;
  state_.wallClockModeEnabled = wall_clock_mode_enabled;
  state_.useSleepBasedWallProfiler =
      wall_clock_mode_enabled && !use_thread_specific_profiler;
  state_.useThreadSpecificProfiler = use_thread_specific_profiler;
  state_.threadDetectIntervalMs = thread_detect_interval_ms;

  state_.enoughStacks = false;
  state_.isLoggerLoopDone = false;

  for (const auto& tracerEntry : state_.tracersMap) {
    if (tracerEntry.first & state_.currentTracers) {
      tracerEntry.second->startTracing();
    }
  }

  if (state_.useSleepBasedWallProfiler) {
    // sleep-based wall-clock profiling is handled by the logger thread
    return true;
  }
  return startProfilingTimers();
}

/**
 * Stop the profiler. Write collected stack traces to profilo
 * The value to write will be a 64 bit <method_id, dex_number>.
 * Unfortunately, DvmDex or DvmHeader doesn't contain a unique dex number that
 * we could reuse. Until this is possibly written custom by redex, we'll use
 * checksum for the dex identification which should collide rare.
 */
void SamplingProfiler::stopProfiling() {
  if (!state_.isProfiling) {
    throw std::logic_error("stopProfiling called while not profiling");
  }

  FBLOGV("Stopping profiling");

  if (state_.useSleepBasedWallProfiler) {
    state_.isLoggerLoopDone.store(true);
  } else {
    if (!stopProfilingTimers()) {
      abort();
    }
    state_.isLoggerLoopDone.store(true);
    int res = sem_post(&state_.slotsCounterSem);
    if (res != 0) {
      FBLOGV("Can not execute sem_post for logger thread");
      errno = 0;
    }
  }

  // Logging errors
  logProfilingErrAnnotation(
      QuickLogConstants::PROF_ERR_SIG_CRASHES, state_.errSigCrashes);
  logProfilingErrAnnotation(
      QuickLogConstants::PROF_ERR_SLOT_MISSES, state_.errSlotMisses);
  logProfilingErrAnnotation(
      QuickLogConstants::PROF_ERR_STACK_OVERFLOWS, state_.errStackOverflows);

  FBLOGV(
      "Stack overflows = %d, Sig crashes = %d, Slot misses = %d",
      state_.errStackOverflows.load(),
      state_.errSigCrashes.load(),
      state_.errSlotMisses.load());

  state_.currentSlot = 0;
  state_.errSigCrashes = 0;
  state_.errSlotMisses = 0;
  state_.errStackOverflows = 0;

  for (const auto& tracerEntry : state_.tracersMap) {
    if (tracerEntry.first & state_.currentTracers) {
      tracerEntry.second->stopTracing();
    }
  }

  unregisterSignalHandlers();

  state_.isProfiling = false;
}

void SamplingProfiler::addToWhitelist(int targetThread) {
  std::unique_lock<std::mutex> lock(state_.whitelist->whitelistedThreadsMtx);
  state_.whitelist->whitelistedThreads.insert(
      static_cast<int32_t>(targetThread));
}

void SamplingProfiler::removeFromWhitelist(int targetThread) {
  std::unique_lock<std::mutex> lock(state_.whitelist->whitelistedThreadsMtx);
  state_.whitelist->whitelistedThreads.erase(targetThread);
}

void SamplingProfiler::resetFrameworkNamesSet() {
  // Let the logger loop know we should reset our cache of frames
  state_.resetFrameworkSymbols.store(true);
}

std::unordered_map<int32_t, std::shared_ptr<BaseTracer>>
SamplingProfiler::ComputeAvailableTracers(uint32_t available_tracers) {
  std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracers;
  if (available_tracers & tracers::DALVIK) {
    tracers[tracers::DALVIK] = std::make_shared<DalvikTracer>();
  }

#if HAS_NATIVE_TRACER && __arm__
  if (available_tracers & tracers::NATIVE) {
    tracers[tracers::NATIVE] = std::make_shared<NativeTracer>();
  }
#endif

  if (available_tracers & tracers::ART_UNWINDC_5_0) {
    tracers[tracers::ART_UNWINDC_5_0] = std::make_shared<ArtUnwindcTracer50>();
  }

  if (available_tracers & tracers::ART_UNWINDC_5_1) {
    tracers[tracers::ART_UNWINDC_5_1] = std::make_shared<ArtUnwindcTracer51>();
  }

  if (available_tracers & tracers::ART_UNWINDC_6_0) {
    tracers[tracers::ART_UNWINDC_6_0] = std::make_shared<ArtUnwindcTracer60>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_0_0) {
    tracers[tracers::ART_UNWINDC_7_0_0] =
        std::make_shared<ArtUnwindcTracer700>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_1_0) {
    tracers[tracers::ART_UNWINDC_7_1_0] =
        std::make_shared<ArtUnwindcTracer710>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_1_1) {
    tracers[tracers::ART_UNWINDC_7_1_1] =
        std::make_shared<ArtUnwindcTracer711>();
  }

  if (available_tracers & tracers::ART_UNWINDC_7_1_2) {
    tracers[tracers::ART_UNWINDC_7_1_2] =
        std::make_shared<ArtUnwindcTracer712>();
  }

  if (available_tracers & tracers::ART_UNWINDC_8_0_0) {
    tracers[tracers::ART_UNWINDC_8_0_0] =
        std::make_shared<ArtUnwindcTracer800>();
  }

  if (available_tracers & tracers::ART_UNWINDC_8_1_0) {
    tracers[tracers::ART_UNWINDC_8_1_0] =
        std::make_shared<ArtUnwindcTracer810>();
  }

  if (available_tracers & tracers::JAVASCRIPT) {
    auto jsTracer = std::make_shared<JSTracer>();
    tracers[tracers::JAVASCRIPT] = jsTracer;
    ExternalTracerManager::getInstance().registerExternalTracer(jsTracer);
  }
  return tracers;
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
