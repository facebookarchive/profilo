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
#include <fbjni/fbjni.h>
#include <fb/xplat_init.h>
#include <sigmuxsetup/sigmuxsetup.h>

#include "profiler/ArtCompatibility.h"
#include "SamplingProfiler.h"
#include "util/common.h"

using namespace facebook::profilo;

const char* CPUProfilerType = "com/facebook/profilo/provider/stacktrace/CPUProfiler";
const char* StackFrameThreadType = "com/facebook/profilo/provider/stacktrace/StackFrameThread";

namespace {

  int32_t getSystemClockTickIntervalMs(facebook::jni::alias_ref<jobject> obj) {
    return systemClockTickIntervalMs();
  }

}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    sigmuxsetup::EnsureSigmuxOverridesArtFaultHandler();

    fbjni::registerNatives(CPUProfilerType,
      {
        makeNativeMethod("nativeInitialize", "(I)Z", profiler::initialize),
        makeNativeMethod("nativeLoggerLoop", "()V", profiler::loggerLoop),
        makeNativeMethod("nativeStopProfiling", "()V", profiler::stopProfiling),
        makeNativeMethod("nativeStartProfiling",
          "(III)Z",
          profiler::startProfiling),
      });
    fbjni::registerNatives(StackFrameThreadType,
      {
        makeNativeMethod(
          "nativeSystemClockTickIntervalMs",
          getSystemClockTickIntervalMs),
      });

    artcompat::registerNatives();
  });
}
