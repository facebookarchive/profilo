// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

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
    std::unordered_set<std::string>& seenLibs);

void unhookLoadedLibs(
    const std::vector<std::pair<char const*, void*>>& functionHooks);
} // namespace hooks
} // namespace profilo
} // namespace facebook
