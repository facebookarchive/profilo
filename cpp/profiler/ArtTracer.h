// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <unistd.h>

#include <fbjni/fbjni.h>
#include "profiler/BaseTracer.h"
#include "profilo/Logger.h"

namespace facebook {
namespace profilo {
namespace profiler {

enum ArtVersion {
  kArt511,
  kArt601,
  kArt700,
};

template <ArtVersion kVersion>
class ArtTracer : public BaseTracer {
public:

  ArtTracer();

  bool collectStack(
      ucontext_t* ucontext,
      int64_t* frames,
      uint8_t& depth,
      uint8_t max_depth) override;

  void flushStack(
    int64_t* frames,
    uint8_t depth,
    int tid,
    int64_t time_) override;

  void stopTracing() override;

  void startTracing() override;
};

#if defined(ART_VERSION_5_1_1)
template class ArtTracer<kArt511>;
using Art51Tracer = ArtTracer<kArt511>;
#endif

#if defined(ART_VERSION_6_0_1)
template class ArtTracer<kArt601>;
using Art6Tracer = ArtTracer<kArt601>;
#endif

#if defined(ART_VERSION_7_0_0)
template class ArtTracer<kArt700>;
using Art70Tracer = ArtTracer<kArt700>;
#endif

} // namespace profiler
} // namespace profilo
} // namespace facebook
