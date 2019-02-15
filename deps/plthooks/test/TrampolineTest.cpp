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

#include <plthooktests/test.h>

#include <gtest/gtest.h>
#include <plthooks/hooks.h>
#include <plthooks/trampoline.h>
#include <sys/mman.h>
#include <sstream>

namespace facebook {
namespace plthooks {
namespace trampoline {

/**
 * The purpose of this test is to run the trampoline code without
 * any of the surrounding allocation & copy logic.
 * This allows us to debug it cleanly and test just the actual trampoline
 * contract.
 */
struct TrampolineTest : public BaseTest {
  static void* test_push_stack(HookId id, void* ret) {
    push_vals = {true, id, ret};
    return (void*) &test_hook;
  }

  static void* test_pop_stack() {
    pop_vals = {true};
    return push_vals.return_address;
  }

  static double test_hook(short a, double b, int c) {
    hook_vals = {true, a, b, c};
    return 0.50;
  }

  virtual void SetUp() {
    push_vals = {};
    hook_vals = {};
    pop_vals = {};
  }

  struct PushVals {
    bool called;
    HookId id;
    void* return_address;
  };
  static PushVals push_vals;

  struct HookVals {
    bool called;
    short a;
    double b;
    int c;
  };
  static HookVals hook_vals;

  struct PopVals {
    bool called;
  };
  static PopVals pop_vals;
};

TrampolineTest::PushVals TrampolineTest::push_vals = {};
TrampolineTest::HookVals TrampolineTest::hook_vals = {};
TrampolineTest::PopVals TrampolineTest::pop_vals = {};

TEST_F(TrampolineTest, testTrampoline) {
  auto trampoline_code = reinterpret_cast<decltype(test_hook)*>(
      trampoline_template_pointer());
  void* trampoline_data = trampoline_data_pointer();

  // Change the permissions around the trampoline code so we can write
  // to its PIC data area.
  auto ret = mprotect(
      reinterpret_cast<void*>(
          reinterpret_cast<uintptr_t>(trampoline_data) & ~(PAGE_SIZE - 1)),
      PAGE_SIZE,
      PROT_READ | PROT_WRITE | PROT_EXEC);

  ASSERT_EQ(ret, 0) << "could not mprotect trampoline data: "
                    << strerror(errno);

  struct __attribute__((packed)) data_fields {
    decltype(test_push_stack)* push_hook;
    decltype(test_pop_stack)* pop_hook;
    HookId id;
  };
  static_assert(
      sizeof(data_fields) == trampoline_data_size(),
      "trampoline data is the wrong size");

  auto fields = reinterpret_cast<data_fields*>(trampoline_data);
  fields->push_hook = &test_push_stack;
  fields->pop_hook = &test_pop_stack;
  fields->id = 0xfaceb00c;

  auto trampoline_return = trampoline_code(10, 0.25, 20);

  EXPECT_EQ(trampoline_return, 0.50) << "return result not the same";
  EXPECT_TRUE(push_vals.called) << "push_hook not called";
  EXPECT_TRUE(hook_vals.called) << "hook not called";
  EXPECT_TRUE(pop_vals.called) << "pop_hook not called";

  EXPECT_EQ(push_vals.id, 0xfaceb00c)
      << "push_hook called with wrong hook id value";
  EXPECT_EQ(hook_vals.a, 10) << "hook_a is wrong";
  EXPECT_EQ(hook_vals.b, 0.25) << "hook_b is wrong";
  EXPECT_EQ(hook_vals.c, 20) << "hook_c is wrong";
}

} // namespace trampoline
} // namespace plthooks
} // namespace facebook
