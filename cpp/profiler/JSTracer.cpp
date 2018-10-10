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

#include "JSTracer.h"
#include "profilo/Logger.h"

namespace facebook {
namespace profilo {
namespace profiler {

bool JSTracer::collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint8_t& depth,
    uint8_t max_depth) {
  return ExternalTracer::collectStack(ucontext, frames, depth, max_depth);
}

void JSTracer::flushStack(
    int64_t* frames,
    uint8_t depth,
    int tid,
    int64_t time_) {
  Logger::get().writeStackFrames(
      tid, time_, frames, depth, entries::JAVASCRIPT_STACK_FRAME);
}

void JSTracer::prepare() {}
void JSTracer::stopTracing() {}
void JSTracer::startTracing() {}

} // namespace profiler
} // namespace profilo
} // namespace facebook
