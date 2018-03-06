// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <profilo/logger/lfrb/LockFreeRingBuffer.h>
#include <profilo/LogEntry.h>

#include "Packet.h"

#define PROFILOEXPORT __attribute__((visibility("default")))

namespace facebook {
namespace profilo {

using TraceBuffer = logger::lfrb::LockFreeRingBuffer<logger::Packet>;

class RingBuffer {

  static const size_t DEFAULT_SLOT_COUNT = 1000;

  public:
  PROFILOEXPORT static TraceBuffer& get(size_t sz = DEFAULT_SLOT_COUNT);
};

} // namespace profilo
} // namespace facebook
