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
#include <stdexcept>

#include <fb/Build.h>
#include <cppdistract/dso.h>

#include <linker/linker.h>
#include <linker/link.h>
#include <linker/sharedlibs.h>

#include <plthooktests/test.h>

static clock_t hook_clock() {
  if (CALL_PREV(hook_clock) == 0) {
    return 0;
  }
  return 0xface;
}

struct OneHookTest : public BaseTest {
  OneHookTest()
    : libtarget(LIBDIR("libtarget.so")) { }

  virtual void SetUp() {
    BaseTest::SetUp();
    ASSERT_EQ(0, hook_plt_method("libtarget.so", "clock", (void*)hook_clock));
  }

  facebook::cppdistract::dso const libtarget;
};

TEST_F(OneHookTest, testHook) {
  auto call_clock = libtarget.get_symbol<int()>("call_clock");
  ASSERT_EQ(0xface, call_clock());
}

struct TwoHookTest : public OneHookTest {
  TwoHookTest()
    : libsecond_hook(LIBDIR("libsecond_hook.so")),
      perform_hook(libsecond_hook.get_symbol<int()>("perform_hook")),
      cleanup(libsecond_hook.get_symbol<int()>("cleanup")) { }

  ~TwoHookTest() {
    cleanup();
  }

  virtual void SetUp() {
    OneHookTest::SetUp();
    ASSERT_EQ(1, perform_hook());
  }

  facebook::cppdistract::dso const libsecond_hook;
  int (* const perform_hook)();
  int (* const cleanup)();
};

TEST_F(TwoHookTest, testDoubleHook) {
  auto call_clock = libtarget.get_symbol<int()>("call_clock");
  ASSERT_EQ(0xfaceb00c, call_clock());
}
