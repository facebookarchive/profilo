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

#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <jni.h>

#include <profilo/jni/JMultiBufferLogger.h>
#include <profilo/profiler/ArtCompatibility.h>
#include <profilo/profiler/BaseTracer.h>
#include <profilo/util/common.h>

#include <profilo/profiler/ArtUnwindcTracer_500.h>
#include <profilo/profiler/ArtUnwindcTracer_510.h>
#include <profilo/profiler/ArtUnwindcTracer_600.h>
#include <profilo/profiler/ArtUnwindcTracer_700.h>
#include <profilo/profiler/ArtUnwindcTracer_710.h>
#include <profilo/profiler/ArtUnwindcTracer_711.h>
#include <profilo/profiler/ArtUnwindcTracer_712.h>
#include <profilo/profiler/ArtUnwindcTracer_800.h>
#include <profilo/profiler/ArtUnwindcTracer_810.h>
#include <profilo/profiler/ArtUnwindcTracer_900.h>
#include <profilo/profiler/DalvikTracer.h>
#include <profilo/profiler/ExternalTracerManager.h>
#include <profilo/profiler/JSTracer.h>

#if HAS_NATIVE_TRACER
#include <profilo/profiler/NativeTracer.h>
#endif

#include "SamplingProfiler.h"

using namespace facebook::profilo;
using namespace facebook::profilo::profiler;
using facebook::profilo::logger::JMultiBufferLogger;

const char* CPUProfilerType =
    "com/facebook/profilo/provider/stacktrace/CPUProfiler";
const char* StackFrameThreadType =
    "com/facebook/profilo/provider/stacktrace/StackFrameThread";
const char* StackTraceWhitelist =
    "com/facebook/profilo/provider/stacktrace/StackTraceWhitelist";

namespace {

int32_t getCpuClockResolutionMicros(facebook::jni::alias_ref<jobject>) {
  return cpuClockResolutionMicros();
}

std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> makeAvailableTracers(
    MultiBufferLogger& logger,
    uint32_t available_tracers,
    bool native_tracer_unwind_dex_frames,
    int32_t native_tracer_unwind_thread_pri,
    size_t native_tracer_unwind_queue_size,
    bool native_tracer_log_partial_stacks) {
  std::unordered_map<int32_t, std::shared_ptr<BaseTracer>> tracers;
  if (available_tracers & tracers::DALVIK) {
    tracers[tracers::DALVIK] = std::make_shared<DalvikTracer>();
  }

#if HAS_NATIVE_TRACER
  if (available_tracers & tracers::NATIVE) {
    tracers[tracers::NATIVE] = std::make_shared<NativeTracer>(
        logger,
        native_tracer_unwind_dex_frames,
        native_tracer_unwind_thread_pri,
        native_tracer_unwind_queue_size,
        native_tracer_log_partial_stacks);
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

  if (available_tracers & tracers::ART_UNWINDC_9_0_0) {
    tracers[tracers::ART_UNWINDC_9_0_0] =
        std::make_shared<ArtUnwindcTracer900>();
  }

  if (available_tracers & tracers::JAVASCRIPT) {
    auto jsTracer = std::make_shared<JSTracer>();
    tracers[tracers::JAVASCRIPT] = jsTracer;
    ExternalTracerManager::getInstance().registerExternalTracer(jsTracer);
  }
  return tracers;
}

static jboolean nativeInitialize(
    fbjni::alias_ref<jobject>,
    JMultiBufferLogger* jlogger,
    jint tracers,
    jboolean native_tracer_unwind_dex_frames,
    jint native_tracer_unwind_thread_pri,
    jint native_tracer_unwind_queue_size,
    jboolean native_tracer_log_partial_stacks) {
  auto available_tracers = static_cast<uint32_t>(tracers);
  auto& logger = jlogger->nativeInstance();
  return SamplingProfiler::getInstance().initialize(
      logger,
      available_tracers,
      makeAvailableTracers(
          logger,
          available_tracers,
          native_tracer_unwind_dex_frames,
          native_tracer_unwind_thread_pri,
          static_cast<size_t>(native_tracer_unwind_queue_size),
          native_tracer_log_partial_stacks));
}

static void nativeLoggerLoop(fbjni::alias_ref<jobject>) {
  return SamplingProfiler::getInstance().loggerLoop();
}

static void nativeStopProfiling(fbjni::alias_ref<jobject>) {
  return SamplingProfiler::getInstance().stopProfiling();
}

static jboolean nativeStartProfiling(
    fbjni::alias_ref<jobject>,
    jint requested_tracers,
    jint sampling_rate_ms,
    jint thread_detect_interval_ms,
    jboolean cpu_clock_mode,
    jboolean wall_clock_mode,
    jboolean new_prof_signal) {
  return SamplingProfiler::getInstance().startProfiling(
      requested_tracers,
      sampling_rate_ms,
      thread_detect_interval_ms,
      cpu_clock_mode,
      wall_clock_mode,
      new_prof_signal);
}

static void nativeResetFrameworkNamesSet(fbjni::alias_ref<jobject>) {
  return SamplingProfiler::getInstance().resetFrameworkNamesSet();
}

static void nativeAddToWhitelist(fbjni::alias_ref<jobject>, jint tid) {
  SamplingProfiler::getInstance().addToWhitelist(tid);
}

static void nativeRemoveFromWhitelist(fbjni::alias_ref<jobject>, jint tid) {
  SamplingProfiler::getInstance().removeFromWhitelist(tid);
}
} // namespace

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    fbjni::registerNatives(
        CPUProfilerType,
        {
            makeNativeMethod("nativeInitialize", nativeInitialize),
            makeNativeMethod("nativeLoggerLoop", nativeLoggerLoop),
            makeNativeMethod("nativeStopProfiling", nativeStopProfiling),
            makeNativeMethod("nativeStartProfiling", nativeStartProfiling),
            makeNativeMethod(
                "nativeResetFrameworkNamesSet", nativeResetFrameworkNamesSet),
        });
    fbjni::registerNatives(
        StackFrameThreadType,
        {
            makeNativeMethod(
                "nativeCpuClockResolutionMicros", getCpuClockResolutionMicros),
        });
    fbjni::registerNatives(
        StackTraceWhitelist,
        {
            makeNativeMethod("nativeAddToWhitelist", nativeAddToWhitelist),
            makeNativeMethod(
                "nativeRemoveFromWhitelist", nativeRemoveFromWhitelist),
        });

    artcompat::registerNatives();
  });
}
