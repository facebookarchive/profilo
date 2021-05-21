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

#include "ArtCompatibility.h"
#include "ArtCompatibilityRunner.h"

#include "profilo/profiler/ArtUnwindcTracer_500.h"
#include "profilo/profiler/ArtUnwindcTracer_510.h"
#include "profilo/profiler/ArtUnwindcTracer_600.h"
#include "profilo/profiler/ArtUnwindcTracer_700.h"
#include "profilo/profiler/ArtUnwindcTracer_710.h"
#include "profilo/profiler/ArtUnwindcTracer_711.h"
#include "profilo/profiler/ArtUnwindcTracer_712.h"
#include "profilo/profiler/ArtUnwindcTracer_800.h"
#include "profilo/profiler/ArtUnwindcTracer_810.h"
#include "profilo/profiler/ArtUnwindcTracer_900.h"
#include "profilo/profiler/BaseTracer.h"

#include <fb/log.h>
#include <fbjni/fbjni.h>

#include <memory>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace artcompat {

namespace {

using namespace profiler::tracers;

jboolean check(JNIEnv* env, jclass, jint tracers) {
  if (tracers & ART_UNWINDC_5_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer50>();
    return runJavaCompatibilityCheck(versions::ANDROID_5, tracer.get());
  } else if (tracers & ART_UNWINDC_5_1) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer51>();
    return runJavaCompatibilityCheck(versions::ANDROID_5, tracer.get());
  } else if (tracers & ART_UNWINDC_6_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer60>();
    return runJavaCompatibilityCheck(versions::ANDROID_6_0, tracer.get());
  } else if (tracers & ART_UNWINDC_7_0_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer700>();
    return runJavaCompatibilityCheck(versions::ANDROID_7_0, tracer.get());
  } else if (tracers & ART_UNWINDC_7_1_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer710>();
    return runJavaCompatibilityCheck(versions::ANDROID_7_0, tracer.get());
  } else if (tracers & ART_UNWINDC_7_1_1) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer711>();
    return runJavaCompatibilityCheck(versions::ANDROID_7_0, tracer.get());
  } else if (tracers & ART_UNWINDC_7_1_2) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer712>();
    return runJavaCompatibilityCheck(versions::ANDROID_7_0, tracer.get());
  } else if (tracers & ART_UNWINDC_8_0_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer800>();
    return runJavaCompatibilityCheck(versions::ANDROID_8_0, tracer.get());
  } else if (tracers & ART_UNWINDC_8_1_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer810>();
    return runJavaCompatibilityCheck(versions::ANDROID_8_1, tracer.get());
  } else if (tracers & ART_UNWINDC_9_0_0) {
    auto tracer = std::make_unique<profiler::ArtUnwindcTracer900>();
    return runJavaCompatibilityCheck(versions::ANDROID_9_0, tracer.get());
  } else {
    return false;
  }
}

} // namespace

void registerNatives() {
  constexpr auto kArtCompatibilityType =
      "com/facebook/profilo/provider/stacktrace/ArtCompatibility";

  fbjni::registerNatives(
      kArtCompatibilityType,
      {
          makeNativeMethod("nativeCheck", check),
      });
}

} // namespace artcompat
} // namespace profilo
} // namespace facebook
