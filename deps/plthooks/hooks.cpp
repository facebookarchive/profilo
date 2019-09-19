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

#include <plthooks/hooks.h>
#include <linker/locks.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace facebook {
namespace plthooks {
namespace hooks {

namespace {

struct InternalHookInfo {
  HookId id;
  uintptr_t got_address;
  std::vector<void*> hooks;

  pthread_rwlock_t mutex;
  InternalHookInfo()
      : id(), got_address(), hooks(), mutex(PTHREAD_RWLOCK_INITIALIZER) {}
};

struct Globals {
  // These are std::map instead of unordered_map because GOT addresses
  // are not sufficiently random for a hash map.
  std::map<HookId, std::shared_ptr<InternalHookInfo>> hooks_by_id;
  std::map<uintptr_t, std::shared_ptr<InternalHookInfo>> hooks_by_got_address;
  pthread_rwlock_t map_mutex;

  std::atomic<HookId> next_id;

  Globals()
      : hooks_by_id(),
        hooks_by_got_address(),
        map_mutex(PTHREAD_RWLOCK_INITIALIZER),
        next_id(1) {}
};

Globals& getGlobals() {
  static Globals globals{};
  return globals;
}

} // namespace

inline HookId allocate_id() {
  return getGlobals().next_id.fetch_add(1);
}

bool is_hooked(uintptr_t got_address) {
  auto& globals = getGlobals();
  auto& map = globals.hooks_by_got_address;
  ReaderLock lock(&globals.map_mutex);
  return map.find(got_address) != map.end();
}

ssize_t list_size(HookId id) {
  auto& globals = getGlobals();
  auto& map = globals.hooks_by_id;
  ReaderLock map_lock(&globals.map_mutex);
  auto it = map.find(id);
  if (it == map.end()) {
    // Hook not registered
    return -1;
  }

  auto& info = *it->second;
  ReaderLock info_lock(&info.mutex);
  return info.hooks.size();
}

std::vector<void*> get_run_list(HookId id) {
  auto& globals = getGlobals();
  auto& map = globals.hooks_by_id;
  ReaderLock map_lock(&globals.map_mutex);
  auto it = map.find(id);
  if (it == map.end()) {
    // Hook not registered
    return {};
  }

  auto& info = *it->second;
  ReaderLock info_lock(&info.mutex);

  return info.hooks;
}

HookResult add(HookInfo& info) {
  auto& globals = getGlobals();
  if (info.previous_function == nullptr || info.new_function == nullptr ||
      info.got_address == 0) {
    return WRONG_HOOK_INFO;
  }

  auto& got_map = globals.hooks_by_got_address;

  // First try just taking the reader lock, in case we already have an entry.
  {
    ReaderLock map_lock(&globals.map_mutex);
    auto it = got_map.find(info.got_address);
    if (it != got_map.end()) {
      // Success, we already have an entry for this GOT address.
      auto& internal_info = *it->second;
      WriterLock info_lock(&internal_info.mutex);
      internal_info.hooks.emplace_back(info.new_function);
      return ALREADY_HOOKED_APPENDED;
    }
  }
  // Okay, we need to do this from scratch.

  auto internal_info = std::make_shared<InternalHookInfo>();
  // No one else can have internal_info so no point taking the writer lock.

  internal_info->id = allocate_id();
  internal_info->got_address = info.got_address;
  internal_info->hooks.emplace_back(info.previous_function);
  internal_info->hooks.emplace_back(info.new_function);

  WriterLock map_lock(&globals.map_mutex);
  globals.hooks_by_got_address.emplace(
      internal_info->got_address, internal_info);
  globals.hooks_by_id.emplace(internal_info->id, internal_info);

  info.out_id = internal_info->id;
  return NEW_HOOK;
}

HookResult remove(HookInfo& info) {
  auto& globals = getGlobals();
  if (info.new_function == nullptr || info.got_address == 0) {
    return WRONG_HOOK_INFO;
  }

  WriterLock map_lock(&globals.map_mutex);
  auto& got_map = globals.hooks_by_got_address;
  auto it = got_map.find(info.got_address);
  if (it == got_map.end()) {
    return UNKNOWN_HOOK;
  }
  // Success, we already have an entry for this GOT address.
  // Take a reference to the shared_ptr to keep the data around
  // as we go about clearing the indices.
  auto internal_info = it->second;
  WriterLock info_lock(&internal_info->mutex);

  if (internal_info->hooks.size() == 1) {
    // Double-check the case where there's only one item left.
    if (internal_info->hooks.at(0) != info.new_function) {
      return WRONG_HOOK_INFO;
    }
    // Okay, we have one item and we want to remove it, let's
    // clear all the data structures.
    globals.hooks_by_got_address.erase(it);

    // We cannot remove this hook from hooks_by_id because another thread
    // may be racing with us and entering the trampoline, so it would need
    // to be able to look things up by id.

    return REMOVED_FULLY;
  }

  auto hooks_it = std::find(
      internal_info->hooks.begin(),
      internal_info->hooks.end(),
      info.new_function);
  if (hooks_it == internal_info->hooks.end()) {
    return UNKNOWN_HOOK;
  }

  if (hooks_it == internal_info->hooks.begin()) {
    // Can't remove the original function if you have hooks after it.
    return WRONG_HOOK_INFO;
  }

  internal_info->hooks.erase(hooks_it);
  auto previous_function = internal_info->hooks.at(0); // original function
  info.previous_function = previous_function;

  return internal_info->hooks.size() == 1 ? REMOVED_TRIVIAL
                                          : REMOVED_STILL_HOOKED;
}

} // namespace hooks
} // namespace plthooks
} // namespace facebook
