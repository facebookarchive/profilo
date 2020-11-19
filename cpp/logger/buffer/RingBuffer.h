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

#include <profilo/logger/buffer/TraceBuffer.h>
#include <profilo/mmapbuf/Buffer.h>

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {

//
// A holder class for the singleton ring buffer. Must be initialized
// by passing in an instance of a Buffer, the actual implementation.
//
class RingBuffer {
 public:
  constexpr static auto kVersion = 1;

  //
  // Set the passed-in buffer as The Buffer.
  //
  PROFILOEXPORT static void init(mmapbuf::Buffer& newBuffer);

  //
  // Cleans-up current buffer and reverts back to no-op mode.
  // DO NOTE USE: This operation is unsafe and currently serve merely as stub
  // for future dynamic buffer management extensions. All tracing should be
  // disabled before this method can be called.
  //
  PROFILOEXPORT static void destroy();

  PROFILOEXPORT static TraceBuffer& get();
};

} // namespace profilo
} // namespace facebook
