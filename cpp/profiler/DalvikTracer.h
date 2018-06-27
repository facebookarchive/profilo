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

#include <dalvik-subset/internals.h>

#include "profiler/BaseTracer.h"
#include "profilo/Logger.h"

namespace facebook {
namespace profilo {
namespace profiler {

using dvmThreadSelf_t = Thread*(*)();

class DalvikTracer : public BaseTracer {
public:

  DalvikTracer();

  DalvikTracer(const DalvikTracer &obj) = delete;
  DalvikTracer& operator=(DalvikTracer obj) = delete;

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

  void prepare() override;

  void stopTracing() override;

  void startTracing() override;


private:
  int64_t getMethodIdForSymbolication(const Method* method);

  dvmThreadSelf_t dvmThreadSelf_;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
