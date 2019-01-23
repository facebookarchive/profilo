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

#include <string.h>
#include "profiler/BaseTracer.h"

namespace facebook {
namespace profilo {
namespace profiler {

struct framework_prefix {
  char const* name;
  size_t length;
};

class JavaBaseTracer : public BaseTracer {
 public:
  virtual bool collectJavaStack(
      ucontext_t* ucontext,
      int64_t* frames,
      char const** method_names,
      char const** class_descriptors,
      uint8_t& depth,
      uint8_t max_depth) = 0;

  static bool isFramework(char const* name) {
    static constexpr framework_prefix prefixes[]{{"Ljava", 5},
                                                 {"Landroid", 8},
                                                 {"Ldalvik", 7},
                                                 {"Lcom/android", 12},
                                                 {"Lorg/apache", 11}};
    constexpr int framework_prefixes_len = 5;
    for (int i = 0; i < framework_prefixes_len; i++) {
      auto& p = prefixes[i];
      if (strncmp(name, p.name, p.length) == 0) {
        return true;
      }
    }
    return false;
  }

  // Keep these in sync with all ART_UNWINDC_* from BaseTracer
  static bool isJavaTracer(int32_t type) {
    return type == tracers::ART_UNWINDC_6_0 ||
        type == tracers::ART_UNWINDC_7_0_0 ||
        type == tracers::ART_UNWINDC_7_1_0 ||
        type == tracers::ART_UNWINDC_7_1_1 ||
        type == tracers::ART_UNWINDC_7_1_2;
  }
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
