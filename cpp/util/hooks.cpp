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

#include <util/hooks.h>

#include <dlfcn.h>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <linker/sharedlibs.h>

namespace facebook {
namespace profilo {
namespace hooks {

// Arguments:
// functionHooks: vector of pairs {"function", ptr_to_function}
//                eg: {{"write", writeHook}, {"read", readHook}},
// allowHookingCb: callback function to give a client control whether to hook a
//                 particular library.
//                 It is mainly used for 2 reasons:
//                 1) Allow the client to blacklist libraries.
//                 2) Avoid hooking the same library twice.
// data: Optional custom data pointer which will be passed to allowHookingCb as
//       a parameter.
void hookLoadedLibs(
    const std::vector<std::pair<char const*, void*>>& functionHooks,
    AllowHookingLibCallback allowHookingCb,
    void* data) {
  std::vector<plt_hook_spec> specs{};
  specs.reserve(functionHooks.size());

  for (auto const& hookPair : functionHooks) {
    char const* functionName = hookPair.first;
    void* hook = hookPair.second;
    specs.emplace_back(nullptr, functionName, hook);
  }

  int ret = hook_all_libs(specs.data(), specs.size(), allowHookingCb, data);

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
