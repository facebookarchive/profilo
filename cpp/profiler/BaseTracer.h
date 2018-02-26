// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <unistd.h>
#include <ucontext.h>

static constexpr const uint32_t kSystemDexId = 0xFFFFFFFF;

namespace facebook {
namespace profilo {
namespace profiler {

namespace tracers {
  enum Tracer: uint8_t {
    DALVIK = 1,
    ART_6_0 = 1 << 1,
    NATIVE = 1 << 2,
    ART_5_1 = 1 << 3,
    ART_7_0 = 1 << 4,
  };
}

class BaseTracer {
public:
  virtual bool collectStack(
    ucontext_t* ucontext,
    int64_t* frames,
    uint8_t& depth,
    uint8_t max_depth) = 0;

  virtual void flushStack(
    int64_t* frames,
    uint8_t depth,
    int tid,
    int64_t time_) = 0;

  virtual void startTracing() = 0;

  virtual void stopTracing() = 0;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
