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

#include <jni.h>
#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <sigmuxsetup/sigmuxsetup.h>

#include "SamplingProfiler.h"
#include "profiler/ArtCompatibility.h"
#include "util/common.h"

using namespace facebook::profilo;

const char* CPUProfilerType =
    "com/facebook/profilo/provider/stacktrace/CPUProfiler";
const char* StackFrameThreadType =
    "com/facebook/profilo/provider/stacktrace/StackFrameThread";
const char* StackTraceWhitelist =
    "com/facebook/profilo/provider/stacktrace/StackTraceWhitelist";

using facebook::profilo::profiler::SamplingProfiler;
namespace {

int32_t getSystemClockTickIntervalMs(facebook::jni::alias_ref<jobject>) {
  return systemClockTickIntervalMs();
}

static jboolean nativeInitialize(fbjni::alias_ref<jobject>, jint arg) {
  auto available_tracers = static_cast<uint32_t>(arg);
  return SamplingProfiler::getInstance().initialize(
      available_tracers,
      SamplingProfiler::ComputeAvailableTracers(available_tracers));
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
    jboolean wall_clock_mode) {
  return SamplingProfiler::getInstance().startProfiling(
      requested_tracers,
      sampling_rate_ms,
      thread_detect_interval_ms,
      wall_clock_mode);
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
    sigmuxsetup::EnsureSigmuxOverridesArtFaultHandler();

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
                "nativeSystemClockTickIntervalMs",
                getSystemClockTickIntervalMs),
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
