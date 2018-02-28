// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <profilo/logger/lfrb/LockFreeRingBuffer.h>

#include "Packet.h"

#define LOOMEXPORT __attribute__((visibility("default")))

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

  LOOMEXPORT void write(void* payload, size_t size);
  LOOMEXPORT PacketBuffer::Cursor writeAndGetCursor(
    void* payload,
    size_t size);

private:
  std::atomic<uint32_t> streamID_;
  PacketBufferProvider provider_;
};

} } } // facebook::profilo::logger
