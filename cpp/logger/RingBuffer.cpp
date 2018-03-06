// Copyright 2004-present Facebook. All Rights Reserved.

#include <profilo/RingBuffer.h>

namespace facebook { namespace profilo {

TraceBuffer& RingBuffer::get(size_t sz) {
  static TraceBuffer instance(sz);
  return instance;
}

} } // namespace facebook::profilo
