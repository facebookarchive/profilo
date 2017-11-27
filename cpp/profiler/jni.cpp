// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <fb/fbjni.h>
#include <fb/xplat_init.h>

#include "profiler/ArtCompatibility_511.h"
#include "profiler/ArtCompatibility_601.h"
#include "profiler/ArtCompatibility_700.h"
#include "SamplingProfiler.h"
#include "util/common.h"

using namespace facebook::loom;

const char* CPUProfilerType = "com/facebook/loom/provider/stacktrace/CPUProfiler";
const char* StackFrameThreadType = "com/facebook/loom/provider/stacktrace/StackFrameThread";

namespace {

  int32_t getSystemClockTickIntervalMs(facebook::jni::alias_ref<jobject> obj) {
    return systemClockTickIntervalMs();
  }

}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
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

    artcompat::registerNatives_5_1_1();
    artcompat::registerNatives_6_0_1();
    artcompat::registerNatives_7_0_0();
  });
}
