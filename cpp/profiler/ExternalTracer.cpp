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

#include "ExternalTracer.h"

namespace facebook {
namespace profilo {
namespace profiler {

bool ExternalTracer::collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint8_t& depth,
    uint8_t max_depth) {
  auto fn = callback_.load(std::memory_order_acquire);
  return fn != nullptr && fn(ucontext, frames, &depth, max_depth);
}

void ExternalTracer::registerCallback(profilo_int_collect_stack_fn callback) {
  callback_.store(callback, std::memory_order_release);
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
