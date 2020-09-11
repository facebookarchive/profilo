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

#include <inttypes.h>

namespace facebook {
namespace profilo {
namespace profiler {

class StackCollectionEntryConverter {
 public:
  static void logRetcode(
      uint32_t retcode,
      int32_t tid,
      int64_t time,
      uint32_t profiler_type);
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
