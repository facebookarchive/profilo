/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#if defined(MUSEUM_VERSION_5_1_1)
template class ArtTracer<kArt511>;
using Art51Tracer = ArtTracer<kArt511>;
#endif

#if defined(MUSEUM_VERSION_6_0_1)
template class ArtTracer<kArt601>;
using Art6Tracer = ArtTracer<kArt601>;
#endif

#if defined(MUSEUM_VERSION_7_0_0)
template class ArtTracer<kArt700>;
using Art70Tracer = ArtTracer<kArt700>;
#endif

} // namespace profiler
} // namespace profilo
} // namespace facebook
