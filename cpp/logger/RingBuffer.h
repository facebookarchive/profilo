// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <loom/logger/lfrb/LockFreeRingBuffer.h>
#include <loom/LogEntry.h>

#include "Packet.h"

#define LOOMEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace loom {

using LoomBuffer = logger::lfrb::LockFreeRingBuffer<logger::Packet>;

class RingBuffer {

  static const size_t DEFAULT_SLOT_COUNT = 1000;

  public:
  LOOMEXPORT static LoomBuffer& get(size_t sz = DEFAULT_SLOT_COUNT);
};

} // namespace loom
} // namespace facebook
