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

#include <plthooks/hooks.h>
#include <stdint.h>

namespace facebook { namespace plthooks {

// Testing only
namespace trampoline {

#if defined(__arm__) || defined(__i386__) || defined(__aarch64__)
#define LINKER_TRAMPOLINE_SUPPORTED_ARCH
#endif

#ifdef LINKER_TRAMPOLINE_SUPPORTED_ARCH
extern "C" {
extern void (*trampoline_template)();
extern void* trampoline_data;
}
#endif

inline void* trampoline_template_pointer() {
#ifdef LINKER_TRAMPOLINE_SUPPORTED_ARCH
  return &trampoline_template;
#else
  return 0;
#endif
}

inline void* trampoline_data_pointer() {
#ifdef LINKER_TRAMPOLINE_SUPPORTED_ARCH
  return &trampoline_data;
#else
  return 0;
#endif
}

constexpr inline size_t trampoline_data_size() {
  return sizeof(void*) * 2 + sizeof(HookId);
}

} // namespace trampoline

void* create_trampoline(HookId hook);

} } // namespace facebook::plthooks

#ifdef __cplusplus
extern "C" {
#endif

void*
get_previous_from_hook(void* hook);

#ifdef __cplusplus
}
#endif
