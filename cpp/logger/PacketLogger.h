// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <profilo/logger/lfrb/LockFreeRingBuffer.h>

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

} } } // facebook::profilo::logger
