// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstring>

#include <yarn/Event.h>
#include <yarn/Records.h>
#include <yarn/detail/Reader.h>

namespace facebook {
namespace yarn {
namespace detail {
namespace parser {

using IdEventMap = std::unordered_map<uint64_t, const Event&>;

void parseBuffer(
  const Event& bufferEvent, 
  IdEventMap& idEventMap,
  RecordListener* listener
);

} // parser
} // detail
} // yarn
} // facebook
