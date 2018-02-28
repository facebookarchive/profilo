// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once


#include <unistd.h>

#include <dalvik-subset/internals.h>

#include "BaseTracer.h"
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

  void stopTracing() override;

  void startTracing() override;

private:
  int64_t getMethodIdForSymbolication(const Method* method);

  dvmThreadSelf_t dvmThreadSelf_;
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
