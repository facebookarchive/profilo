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
#include <link.h>
#include <gtest/gtest.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <linker/linker.h>

typedef clock_t (*call_clock_fn)();

static clock_t hook_call_clock() {
  return 0xface;
}

bool
ends_with(const char* str, const char* ending) {
  size_t str_len = strlen(str);
  size_t ending_len = strlen(ending);

  if (ending_len > str_len) {
    return false;
  }
  return strcmp(str + (str_len - ending_len), ending) == 0;
}

void get_full_soname(char* soname) {
  FILE* f = fopen("/proc/self/maps", "r");
  char line[512]{};
  while (fgets(line, 512, f) != nullptr) {
    sscanf(
        line,
        "%*x-%*x %*s %*x %*s %*d %s",
        soname
    );
    if (ends_with(soname, "libtarget.so")) {
      break;
    }
  }
}

TEST(PltHook, testHook) {
  void* handle = dlopen("libtarget.so", RTLD_LOCAL);
  EXPECT_NE(nullptr, handle) << "Error: " << dlerror();

  // We have to call this AFTER we dlopen() the library so that it will
  // actually show up during dl_iterate_phdr or in /proc/self/maps
  EXPECT_EQ(0, linker_initialize());

  call_clock_fn fn = (call_clock_fn) dlsym(handle, "call_clock");

  char soname[512]{};
  get_full_soname(soname);
  int ret = hook_plt_method(handle, soname, "clock", (void*)hook_call_clock);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0xface, fn());

  dlclose(handle);
}
