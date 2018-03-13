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

#include <dlfcn.h>
#include <gtest/gtest.h>
#include <time.h>

#include <linker/linker.h>

typedef clock_t (*call_clock_fn)();

static clock_t hook_call_clock() {
  return 0xface;
}

TEST(PltHook, testHook) {
  EXPECT_EQ(0, linker_initialize());

  void* handle = dlopen("libtarget.so", RTLD_LOCAL);
  EXPECT_NE(nullptr, handle) << "Error: " << dlerror();

  call_clock_fn fn = (call_clock_fn) dlsym(handle, "call_clock");

  EXPECT_EQ(0, hook_plt_method(handle, "libtarget.so", "clock", (void*) hook_call_clock));

  EXPECT_EQ(0xface, fn());

  dlclose(handle);
}
