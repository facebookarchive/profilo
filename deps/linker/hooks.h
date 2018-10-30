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

#include <stdint.h>
#include <unistd.h>
#include <vector>

using HookId = uint32_t;

namespace facebook {
namespace linker {
namespace hooks {

struct HookInfo {
  HookId out_id;
  uintptr_t got_address;
  void* new_function;
  void* previous_function;
};

enum HookResult {
  WRONG_HOOK_INFO = -1,
  NEW_HOOK = 1,
  ALREADY_HOOKED_APPENDED = 2,
};

HookResult add(HookInfo&);

bool is_hooked(uintptr_t got_address);

ssize_t list_size(HookId);

std::vector<void*> get_run_list(HookId id);

// Testing only!
void forget_all();

} // namespace hooks
} // namespace linker
} // namespace facebook
