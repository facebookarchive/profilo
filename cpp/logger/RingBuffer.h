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

#include <profilo/LogEntry.h>
#include <profilo/logger/lfrb/LockFreeRingBuffer.h>

#include "Packet.h"

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {

using TraceBuffer = logger::lfrb::LockFreeRingBuffer<logger::Packet>;

class RingBuffer {
  static const size_t DEFAULT_SLOT_COUNT = 1000;

 public:
  PROFILOEXPORT static TraceBuffer& init(size_t sz = DEFAULT_SLOT_COUNT);
  PROFILOEXPORT static TraceBuffer& get();
};

} // namespace profilo
} // namespace facebook
