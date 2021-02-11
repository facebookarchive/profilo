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

#include <counters/ProcFs.h>
#include <fbjni/fbjni.h>
#include <profilo/MultiBufferLogger.h>
#include <profilo/jni/JMultiBufferLogger.h>

#include "ProcessCounters.h"
#include "SystemCounters.h"
#include "ThreadCounters.h"

using facebook::profilo::logger::JMultiBufferLogger;
using facebook::profilo::logger::MultiBufferLogger;

namespace facebook {
namespace profilo {
namespace counters {

class SystemCounterThread
    : public facebook::jni::HybridClass<SystemCounterThread> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/provider/systemcounters/SystemCounterThread;";

  static facebook::jni::local_ref<jhybriddata> initHybrid(
      facebook::jni::alias_ref<jobject>,
      JMultiBufferLogger* logger);

  static void registerNatives();

  void logCounters();

  void logHighFrequencyThreadCounters();

  void logTraceAnnotations();

 private:
  friend HybridBase;

  SystemCounterThread(MultiBufferLogger& logger);

  MultiBufferLogger& logger_;

  ThreadCounters threadCounters_;
  ProcessCounters processCounters_;
  SystemCounters systemCounters_;

  int32_t extraAvailableCounters_;
  bool highFrequencyMode_;

  void setHighFrequencyMode(bool enabled) {
    highFrequencyMode_ = enabled;
  }
};

} // namespace counters
} // namespace profilo
} // namespace facebook
