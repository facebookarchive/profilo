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
#include "profilo/logger/buffer/RingBuffer.h"

namespace facebook {
namespace profilo {
namespace profiler {

StackCollectionRetcode JSTracer::collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint16_t& depth,
    uint16_t max_depth) {
  return ExternalTracer::collectStack(ucontext, frames, depth, max_depth);
}

void JSTracer::flushStack(
    int64_t* frames,
    uint16_t depth,
    int tid,
    int64_t time_) {
  RingBuffer::get().logger().write(FramesEntry{
      .id = 0,
      .type = EntryType::JAVASCRIPT_STACK_FRAME,
      .timestamp = time_,
      .tid = tid,
      .matchid = 0,
      .frames = {.values = const_cast<int64_t*>(frames), .size = depth}});
}

void JSTracer::prepare() {}
void JSTracer::stopTracing() {}
void JSTracer::startTracing() {}

} // namespace profiler
} // namespace profilo
} // namespace facebook
