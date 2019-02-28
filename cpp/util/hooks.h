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

#include <plthooks/plthooks.h>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace facebook {
namespace profilo {
namespace hooks {

/**
 * Exported functions
 */

/**
 * Hooks all the shared libraries loaded at the time of calling this function.
 * Installs hooks for every {func_name, hook} pair in <functionHooks>,
 * avoiding hooking libs that have been already hooked or that the client
 * doesn't want to hook.
 */
void hookLoadedLibs(
    const std::vector<std::pair<char const*, void*>>& functionHooks,
    AllowHookingLibCallback allowHookingCb,
    void* data);

void unhookLoadedLibs(
    const std::vector<std::pair<char const*, void*>>& functionHooks);
} // namespace hooks
} // namespace profilo
} // namespace facebook
