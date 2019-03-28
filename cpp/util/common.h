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
#include <atomic>
#include <type_traits>

namespace facebook {
namespace profilo {

int64_t monotonicTime();
int32_t threadID();
// Returns 0 if value was not found, and 1 if value <= 1, actual value otherwise
int32_t systemClockTickIntervalMs();

// Given a path, create the directory specified by it, along with all
// intermediate directories
void mkdirs(char const* dir);

} // namespace profilo
} // namespace facebook
