// Copyright 2004-present Facebook. All Rights Reserved.

#include <profilo/RingBuffer.h>

namespace facebook { namespace profilo {

LoomBuffer& RingBuffer::get(size_t sz) {
  static LoomBuffer instance(sz);
  return instance;
}

} } // namespace facebook::profilo
