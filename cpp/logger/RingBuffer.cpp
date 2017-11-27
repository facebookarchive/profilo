// Copyright 2004-present Facebook. All Rights Reserved.

#include <loom/RingBuffer.h>

namespace facebook { namespace loom {

LoomBuffer& RingBuffer::get(size_t sz) {
  static LoomBuffer instance(sz);
  return instance;
}

} } // namespace facebook::loom
