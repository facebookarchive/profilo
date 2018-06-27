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
#include <atomic>
#include <unistd.h>
#include <fb/log.h>

#include "profiler/BaseTracer.h"
#include "profiler/ArtUnwindcTracer.h"
#include "profilo/Logger.h"

#include "profiler/unwindc/runtime.h"
#include "profiler/unwindc/unwinder.h"

namespace facebook {
namespace profilo {
namespace profiler {
#ifdef ANDROID_VERSION_600
namespace android_600 {
#else
#error Unknown Android version
#endif

namespace {

struct unwinder_data {
  ucontext_t* ucontext;
  int64_t* frames;
  uint8_t depth;
  uint8_t max_depth;
};


bool unwind_cb(uintptr_t frame, void* data) {
  unwinder_data* ud = reinterpret_cast<unwinder_data*>(data);
  if (ud->depth >= ud->max_depth) {
    // stack overflow, stop the traversal
    return false;
  }
  ud->frames[ud->depth++] = get_method_trace_id(frame);
  return true;
}

} // anonymous

ArtUnwindcTracer::ArtUnwindcTracer() {}

bool ArtUnwindcTracer::collectStack(
      ucontext_t* ucontext,
      int64_t* frames,
      uint8_t& depth,
      uint8_t max_depth) {
  unwinder_data data {
    .ucontext = ucontext,
    .frames = frames,
    .depth = 0,
    .max_depth = max_depth,
  };
  depth = 0;
  if (!unwind(&unwind_cb, &data)) {
    return false;
  }
  depth = data.depth;
  return true;
}

void ArtUnwindcTracer::flushStack(
    int64_t* frames,
    uint8_t depth,
    int tid,
    int64_t time_) {

  Logger::get().writeStackFrames(
    tid,
    static_cast<int64_t>(time_),
    frames,
    depth);
}

void ArtUnwindcTracer::startTracing() {
  prepare();
}

void ArtUnwindcTracer::stopTracing() {
}

void ArtUnwindcTracer::prepare() {
  // Preinitialize static state.

  (void) get_art_thread();
  (void) get_runtime();
}

} // namespace android_*
} // namespace profiler
} // namespace profilo
} // namespace facebook
