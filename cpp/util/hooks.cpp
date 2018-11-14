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

#include <linker/linker.h>

#include <dlfcn.h>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <util/hooks.h>

namespace facebook {
namespace profilo {

namespace {

bool allowHookingCb(char const* libname, void* data) {
  std::unordered_set<std::string>* seenLibs =
      static_cast<std::unordered_set<std::string>*>(data);

  if (seenLibs->find(libname) != seenLibs->cend()) {
    // We already hooked (or saw and decided not to hook) this library.
    return false;
  }
  seenLibs->insert(libname);
  return true;
}

} // anonymous namespace

namespace hooks {

// Arguments:
// functionHooks: vector of pairs {"function", ptr_to_function}
//                eg: {{"write", writeHook}, {"read", readHook}},
// seenLibs: libraries that we have seen (might or might not be hooked). This
//           is used for 2 reasons:
//           1) Allow the client to blacklist libraries.
//           2) Avoid hooking the same library twice.
void hookLoadedLibs(
    const std::vector<std::pair<char const*, void*>>& functionHooks,
    std::unordered_set<std::string>& seenLibs) {
  std::vector<plt_hook_spec> specs{};
  specs.reserve(functionHooks.size());

  for (auto const& hookPair : functionHooks) {
    char const* functionName = hookPair.first;
    void* hook = hookPair.second;
    specs.emplace_back(nullptr, functionName, hook);
  }

  int ret = hook_all_libs(
      specs.data(),
      specs.size(),
      allowHookingCb,
      static_cast<void*>(&seenLibs));

  if (ret) {
    throw std::runtime_error("Could not hook libraries");
  }
}

void unhookLoadedLibs(
    const std::vector<std::pair<char const*, void*>>& functionHooks) {
  std::vector<plt_hook_spec> specs{};
  specs.reserve(functionHooks.size());

  for (auto const& hookPair : functionHooks) {
    char const* functionName = hookPair.first;
    void* hook = hookPair.second;
    specs.emplace_back(nullptr, functionName, hook);
  }

  int ret = unhook_all_libs(specs.data(), specs.size());

  if (ret) {
    throw std::runtime_error("Could not unhook libraries");
  }
}

} // namespace hooks
} // namespace profilo
} // namespace facebook
