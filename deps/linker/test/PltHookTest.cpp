// Copyright 2004-present Facebook. All Rights Reserved.

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
