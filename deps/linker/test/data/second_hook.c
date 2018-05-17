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
#include <time.h>

clock_t second_hook_clock() {
  return (CALL_PREV(second_hook_clock, clock_t(*)()) << 16) | 0xb00c;
}

static void* handle;

int perform_hook() {
  handle = dlopen("libtarget.so", RTLD_LOCAL);
  if (handle == 0) {
    return 0;
  }

  if (linker_initialize() != 0) {
    return 0;
  }

  if (hook_plt_method("libtarget.so", "clock", (void*)second_hook_clock) != 0) {
    return 0;
  }

  return 1;
}

int cleanup() {
  if (dlclose(handle) != 0) {
    return 0;
  }

  return 1;
}
