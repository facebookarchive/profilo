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

#include <profiler/ExternalTracer.h>
#include <profiler/JavaBaseTracer.h>
#include <profiler/Retcode.h>
#include <profilo/ExternalApi.h>

#include <profilo/LogEntry.h>
#include <profilo/TraceProviders.h>

#include <util/common.h>

using namespace facebook::jni;

namespace facebook {
namespace profilo {
namespace profiler {

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

void SamplingProfiler::FaultHandler(
    int signum,
    siginfo_t* siginfo,
    void* ucontext) {
  auto scope = SignalHandler::EnterHandler(signum);
  if (!scope.IsEnabled()) {
    scope.CallPreviousHandler(signum, siginfo, ucontext);
    return;
  }

  ProfileState& state = ((SamplingProfiler*)scope.GetData())->state_;

  uint64_t tid = threadID();
  uint64_t targetBusyState = (tid << 16) | StackSlotState::BUSY_WITH_METADATA;

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
    scope.siglongjmp(state.stacks[max_idx].sig_jmp_buf, 1);
  } else {
    scope.CallPreviousHandler(signum, siginfo, ucontext);
  }
}

void SamplingProfiler::maybeSignalReader() {
  uint32_t prevSlotCounter = state_.fullSlotsCounter.fetch_add(1);
  if ((prevSlotCounter + 1) % FLUSH_STACKS_COUNT == 0) {
    int res = sem_post(&state_.slotsCounterSem);
    if (res != 0) {
      abort(); // Something went wrong
    }
  }
}

// Finds the next FREE slot and atomically sets its state to BUSY, so that
// the acquiring thread can safely write to it, and returns the index via
// <outSlot>. Returns true if a FREE slot was found, false otherwise.
bool getSlotIndex(ProfileState& state_, uint64_t tid, uint32_t& outSlot) {
  auto slotIndex = state_.currentSlot.fetch_add(1);
  for (int i = 0; i < MAX_STACKS_COUNT; i++) {
    auto nextSlotIndex = (slotIndex + i) % MAX_STACKS_COUNT;
    auto& slot = state_.stacks[nextSlotIndex];
    uint64_t expected = StackSlotState::FREE;
    uint64_t targetBusyState = (tid << 16) | StackSlotState::BUSY;
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

void SamplingProfiler::UnwindStackHandler(
    int signum,
    siginfo_t* siginfo,
    void* ucontext) {
  auto scope = SignalHandler::EnterHandler(signum);
  if (!scope.IsEnabled()) {
    return;
  }

  SamplingProfiler& profiler = *(SamplingProfiler*)scope.GetData();
  ProfileState& state = profiler.state_;

  uint64_t tid = threadID();
  uint64_t busyState = (tid << 16) | StackSlotState::BUSY_WITH_METADATA;

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
                      (ucontext_t*)ucontext,
                      slot.frames,
                      slot.method_names,
                      slot.class_descriptors,
                      slot.depth,
                      MAX_STACK_DEPTH);
      } else {
        ret = tracerEntry.second->collectStack(
            (ucontext_t*)ucontext, slot.frames, slot.depth, MAX_STACK_DEPTH);
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

      auto nextSlotState = (tid << 16) | ret;
      // In case if a Tracer class handles collection on it's own the slot is
      // freed after the signal is processed.
      if (ret == StackCollectionRetcode::IGNORE) {
        nextSlotState = StackSlotState::FREE;
      }

      if (!slot.state.compare_exchange_strong(busyState, nextSlotState)) {
        // Slot was overwritten by another thread.
        // This is an ordering violation, so abort.
        abortWithReason(
            "Invariant violation - BUSY_WITH_METADATA to return code failed");
      }
      if (nextSlotState != StackSlotState::FREE) {
        profiler.maybeSignalReader();
      }
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
}

void SamplingProfiler::registerSignalHandlers() {
  //
  // Register a handler for SIGPROF.
  //
  // Also, register a handler for SIGSEGV and SIGBUS, so that we can safely
  // jump away in the case of a crash in our SIGPROF handler.
  //
  signal_handlers_.sigprof =
      &SignalHandler::Initialize(SIGPROF, UnwindStackHandler);
  signal_handlers_.sigsegv = &SignalHandler::Initialize(SIGSEGV, FaultHandler);
  signal_handlers_.sigbus = &SignalHandler::Initialize(SIGBUS, FaultHandler);

  signal_handlers_.sigbus->SetData(this);
  signal_handlers_.sigbus->Enable();

  signal_handlers_.sigsegv->SetData(this);
  signal_handlers_.sigsegv->Enable();

  signal_handlers_.sigprof->SetData(this);
  signal_handlers_.sigprof->Enable();
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
  // By waiting for all profiling handlers to finish (which Disable
  // does internally), we solve a), c), and d) (pending fault signals during a
  // profiling signal means we won't exit the corresponding profiling handler
  // until we've handled the fault).
  //
  // We solve b) by never unregistering our signal handler.
  // Once registered, we will bail out on the HandlerScope::IsEnabled check and
  // all will be well on the normal path.
  //

  signal_handlers_.sigprof->Disable();
  signal_handlers_.sigbus->Disable();
  signal_handlers_.sigsegv->Disable();
}

void SamplingProfiler::flushStackTraces(
    std::unordered_set<uint64_t>& loggedFramesSet) {
  int processedCount = 0;
  auto& logger = *state_.logger;
  for (size_t i = 0; i < MAX_STACKS_COUNT; i++) {
    auto& slot = state_.stacks[i];

    uint64_t slotStateCombo = slot.state.load();
    uint16_t slotState = slotStateCombo & 0xffff;
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
        tracer->flushStack(logger, slot.frames, slot.depth, tid, slot.time);
      } else {
        StackCollectionEntryConverter::logRetcode(
            logger, slotState, tid, slot.time, slot.profilerType);
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
            entry.type = EntryType::JAVA_FRAME_NAME;
            entry.extra = slot.frames[i];
            int32_t id = logger.write(std::move(entry));

            std::string full_name{slot.class_descriptors[i]};
            full_name += slot.method_names[i];
            logger.writeBytes(
                EntryType::STRING_VALUE,
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

    uint64_t expected = slotStateCombo;
    // Release the slot
    if (!slot.state.compare_exchange_strong(expected, StackSlotState::FREE)) {
      // Slot was re-used in the middle of the processing by another thread.
      // Aborting.
      abort();
    }
    processedCount++;
  }
}

void logProfilingErrAnnotation(
    MultiBufferLogger& logger,
    int32_t key,
    uint16_t value) {
  if (value == 0) {
    return;
  }
  logger.write(StandardEntry{
      .id = 0,
      .type = EntryType::TRACE_ANNOTATION,
      .timestamp = monotonicTime(),
      .tid = threadID(),
      .callid = key,
      .matchid = 0,
      .extra = value,
  });
}

/**
 * Initializes the profiler. Registers handler for custom defined SIGPROF
 * symbol which will collect traces and inits thread/process ids
 */
bool SamplingProfiler::initialize(
    MultiBufferLogger& logger,
    int32_t available_tracers,
    std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracers) {
  state_.processId = getpid();
  state_.logger = &logger;
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
  int res = 0;
  std::unordered_set<uint64_t> loggedFramesSet{};

  do {
    res = sem_wait(&state_.slotsCounterSem);
    if (res == 0) {
      flushStackTraces(loggedFramesSet);
    }
  } while (!state_.isLoggerLoopDone && (res == 0 || errno == EINTR));
  FBLOGV("Logger thread is shutting down...");
}

bool SamplingProfiler::startProfilingTimers() {
  FBLOGI("Starting profiling timers w/sample rate %d", state_.samplingRateMs);
  state_.timerManager.reset(new TimerManager(
      state_.threadDetectIntervalMs,
      state_.samplingRateMs,
      state_.wallClockModeEnabled,
      state_.wallClockModeEnabled ? state_.whitelist : nullptr));
  state_.timerManager->start();
  return true;
}

bool SamplingProfiler::stopProfilingTimers() {
  state_.timerManager->stop();
  state_.timerManager.reset();
  return true;
}

bool SamplingProfiler::startProfiling(
    int requested_tracers,
    int sampling_rate_ms,
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

  constexpr auto kMinThreadDetectIntervalMs = 7; // TODO_YM T63620953
  if (thread_detect_interval_ms < kMinThreadDetectIntervalMs) {
    thread_detect_interval_ms = kMinThreadDetectIntervalMs;
  }

  state_.samplingRateMs = sampling_rate_ms;
  state_.wallClockModeEnabled = wall_clock_mode_enabled;
  state_.threadDetectIntervalMs = thread_detect_interval_ms;

  state_.isLoggerLoopDone = false;

  for (const auto& tracerEntry : state_.tracersMap) {
    if (tracerEntry.first & state_.currentTracers) {
      tracerEntry.second->startTracing();
    }
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

  if (!stopProfilingTimers()) {
    abort();
  }
  state_.isLoggerLoopDone.store(true);
  int res = sem_post(&state_.slotsCounterSem);
  if (res != 0) {
    FBLOGV("Can not execute sem_post for logger thread");
    errno = 0;
  }

  // Logging errors
  logProfilingErrAnnotation(
      *state_.logger,
      QuickLogConstants::PROF_ERR_SIG_CRASHES,
      state_.errSigCrashes);
  logProfilingErrAnnotation(
      *state_.logger,
      QuickLogConstants::PROF_ERR_SLOT_MISSES,
      state_.errSlotMisses);
  logProfilingErrAnnotation(
      *state_.logger,
      QuickLogConstants::PROF_ERR_STACK_OVERFLOWS,
      state_.errStackOverflows);

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

} // namespace profiler
} // namespace profilo
} // namespace facebook
