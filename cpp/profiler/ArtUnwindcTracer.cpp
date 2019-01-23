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
#include <fb/log.h>
#include <unistd.h>
#include <atomic>

#include "profiler/ArtUnwindcTracer.h"
#include "profiler/BaseTracer.h"
#include "profilo/Logger.h"

#include "profiler/unwindc/runtime.h"

namespace facebook {
namespace profilo {
namespace profiler {

#if ANDROID_VERSION_NUM == 600
static constexpr ArtUnwindcVersion kVersion = kArtUnwindc600;
#elif ANDROID_VERSION_NUM == 700
static constexpr ArtUnwindcVersion kVersion = kArtUnwindc700;
#elif ANDROID_VERSION_NUM == 710
static constexpr ArtUnwindcVersion kVersion = kArtUnwindc710;
#elif ANDROID_VERSION_NUM == 711
static constexpr ArtUnwindcVersion kVersion = kArtUnwindc711;
#elif ANDROID_VERSION_NUM == 712
static constexpr ArtUnwindcVersion kVersion = kArtUnwindc712;
#endif

/**
 * We need to namespace the codegen'd symbols in unwinder.h to appease the
 * linker.
 *
 * Since this compilation unit will only ever have one of these versions
 * defined at a time, we can just ignore the namespacing by means of
 * `using namespace`.
 */
namespace ANDROID_NAMESPACE { // ANDROID_NAMESPACE is a preprocessor variable
#include "profiler/unwindc/unwinder.h"
} // namespace ANDROID_NAMESPACE

using namespace ANDROID_NAMESPACE;

namespace {

struct unwinder_data {
  ucontext_t* ucontext;
  int64_t* frames;
  char const** method_names;
  char const** class_descriptors;
  uint8_t depth;
  uint8_t max_depth;
};

bool unwind_cb(uintptr_t frame, void* data) {
  unwinder_data* ud = reinterpret_cast<unwinder_data*>(data);
  if (ud->depth >= ud->max_depth) {
    // stack overflow, stop the traversal
    return false;
  }
  ud->frames[ud->depth] = get_method_trace_id(frame);

  if (ud->method_names != nullptr && ud->class_descriptors != nullptr) {
    auto declaring_class = get_declaring_class(frame);
    auto class_string_t = get_class_descriptor(declaring_class);
    auto method_string_t = get_method_name(frame);
    ud->method_names[ud->depth] = method_string_t.data;
    ud->class_descriptors[ud->depth] = class_string_t.data;
  }

  ++ud->depth;
  return true;
}

} // namespace

template <>
ArtUnwindcTracer<kVersion>::ArtUnwindcTracer() {}

template <>
bool ArtUnwindcTracer<kVersion>::collectJavaStack(
    ucontext_t* ucontext,
    int64_t* frames,
    char const** method_names,
    char const** class_descriptors,
    uint8_t& depth,
    uint8_t max_depth) {
  unwinder_data data{
      .ucontext = ucontext,
      .frames = frames,
      .method_names = method_names,
      .class_descriptors = class_descriptors,
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

template <>
bool ArtUnwindcTracer<kVersion>::collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint8_t& depth,
    uint8_t max_depth) {
  return collectJavaStack(ucontext, frames, nullptr, nullptr, depth, max_depth);
}

template <>
void ArtUnwindcTracer<kVersion>::flushStack(
    int64_t* frames,
    uint8_t depth,
    int tid,
    int64_t time_) {
  Logger::get().writeStackFrames(
      tid, static_cast<int64_t>(time_), frames, depth);
}

template <>
void ArtUnwindcTracer<kVersion>::prepare() {
  // Preinitialize static state.

  (void)get_art_thread();
}

template <>
void ArtUnwindcTracer<kVersion>::startTracing() {
  prepare();
}

template <>
void ArtUnwindcTracer<kVersion>::stopTracing() {}

} // namespace profiler
} // namespace profilo
} // namespace facebook
