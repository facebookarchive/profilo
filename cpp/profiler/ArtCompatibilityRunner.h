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

#include "profiler/BaseTracer.h"

namespace facebook {
namespace profilo {
namespace artcompat {

namespace versions {
enum AndroidVersion : uint8_t {
  ANDROID_6_0,
  ANDROID_7_0,
};
} // namespace versions

bool runJavaCompatibilityCheck(
    versions::AndroidVersion version,
    profiler::BaseTracer* tracer);

} // namespace artcompat
} // namespace profilo
} // namespace facebook
