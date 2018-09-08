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

#include <profilo/logger/lfrb/LockFreeRingBuffer.h>
#include <functional>

#include "Packet.h"

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {
class Logger;

namespace logger {

using PacketBuffer = lfrb::LockFreeRingBuffer<Packet>;
using PacketBufferProvider = std::function<PacketBuffer&()>;

class PacketLogger {
 public:
  PacketLogger(PacketBufferProvider provider);
  PacketLogger(const PacketLogger& other) = delete;

  PROFILOEXPORT void write(void* payload, size_t size);
  PROFILOEXPORT PacketBuffer::Cursor writeAndGetCursor(
      void* payload,
      size_t size);

 private:
  std::atomic<uint32_t> streamID_;
  PacketBufferProvider provider_;
};

} // namespace logger
} // namespace profilo
} // namespace facebook
