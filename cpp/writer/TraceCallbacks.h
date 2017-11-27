
// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <loom/writer/AbortReason.h>

namespace facebook {
namespace loom {
namespace writer {

struct TraceCallbacks {

  virtual ~TraceCallbacks(){}

  virtual void onTraceStart(
    int64_t trace_id,
    int32_t flags,
    std::string trace_file
  ) = 0;

  virtual void onTraceEnd(int64_t trace_id) = 0;
  
  virtual void onTraceAbort(int64_t trace_id, AbortReason reason) = 0;
};

} // namespace writer
} // namespace loom
} // namespace facebook
