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

#include <errno.h>
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

  // Confusingly enough, this defines the setup for the entire class (i.e. it's
  // only called once).
  static void SetUpTestCase() {
    // Change the permissions around the trampoline code so we can write
    // to its PIC data area.
    trampoline_data = reinterpret_cast<data_fields*>(trampoline_data_pointer());
    static_assert(
        sizeof(data_fields) == trampoline_data_size(),
        "trampoline data is the wrong size");

    auto ret = mprotect(
        reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(trampoline_data) & ~(PAGE_SIZE - 1)),
        PAGE_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC);

    ASSERT_EQ(ret, 0) << "could not mprotect trampoline data: "
                      << strerror(errno);
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

  struct __attribute__((packed)) data_fields {
    decltype(test_push_stack)* push_hook;
    decltype(test_pop_stack)* pop_hook;
    HookId id;
  };
  static data_fields* trampoline_data;
};

TrampolineTest::PushVals TrampolineTest::push_vals = {};
TrampolineTest::HookVals TrampolineTest::hook_vals = {};
TrampolineTest::PopVals TrampolineTest::pop_vals = {};
TrampolineTest::data_fields* TrampolineTest::trampoline_data;

TEST_F(TrampolineTest, testTrampoline) {
  auto trampoline_code = reinterpret_cast<decltype(test_hook)*>(
      trampoline_template_pointer());

  trampoline_data->push_hook = &test_push_stack;
  trampoline_data->pop_hook = &test_pop_stack;
  trampoline_data->id = 0xfaceb00c;

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

#if defined(__aarch64__)

asm(".data\n"
    ".zeroes:\n"
    ".quad 0\n"
    ".quad 0\n"
    ".quad 0\n"
    ".quad 0\n");

// Test AAPCS64 conformance.
// https://github.com/ARM-software/abi-aa/blob/2019Q4/aapcs64/aapcs64.rst
struct TrampolineAbiTest : public TrampolineTest {
  static void* test_push_stack(HookId id, void* ret) {
    push_vals = {true, id, ret};

    // Clobber all arguments used for register passing, to ensure the trampoline
    // is saving them.
    asm("adr   x16, .zeroes\n"
        // AAPCS64 6.4: Integral arguments and small aggregates are passed in
        // r0-r7.
        "ldp   x0, x1, [x16]\n"
        "ldp   x2, x3, [x16]\n"
        "ldp   x4, x5, [x16]\n"
        "ldp   x6, x7, [x16]\n"
        // AAPCS64 6.5: If a function's return value can't be passed in a
        // register, the caller allocates memory to hold it and passes its
        // address as an argument in x8.
        "ldr   x8, [x16]\n"
        // AAPCS64 6.4: Floating point arguments and homogenous floating
        // point/vector aggregates are passed in v0-v7.
        "ldp   q0, q1, [x16]\n"
        "ldp   q2, q3, [x16]\n"
        "ldp   q4, q5, [x16]\n"
        "ldp   q6, q7, [x16]\n"
        :
        :
        : "x0",
          "x1",
          "x2",
          "x3",
          "x4",
          "x5",
          "x6",
          "x7",
          "x8",
          "x16",
          "v0",
          "v1",
          "v2",
          "v3",
          "v4",
          "v5",
          "v6",
          "v7");

    return test_hook_ptr;
  }

  static void* test_pop_stack() {
    // Clobber all arguments used for result returns, to ensure the trampoline
    // is saving them.
    asm("adr   x16, .zeroes\n"
        // AAPCS64 6.4: Aggregates up to 16 bytes can be allocated to general
        // purpose registers, so x0-x1 might be used for returns.
        "ldp   x0, x1, [x16]\n"
        // AAPCS 5.6.5.1: A homogenous floating-point aggregate (HFA) is an
        // aggregate of up to four members where all members have the same
        // floating point type. HFAs are passed and returned using the
        // floating-point registers, so q0-q3 might be used for returns.
        "ldp   q0, q1, [x16]\n"
        "ldp   q2, q3, [x16]\n"
        :
        :
        : "x0", "x1", "x16", "v0", "v1", "v2", "v3");

    pop_vals = {true};
    return push_vals.return_address;
  }

  static void SetUpTestCase() {
    TrampolineTest::SetUpTestCase();
  }

  virtual void SetUp() override {
    trampoline_data->push_hook = &test_push_stack;
    trampoline_data->pop_hook = &test_pop_stack;
    trampoline_data->id = 0xfaceb00c;
  }

  static long integer_return() {
    return 42;
  }

  static double float_return() {
    return 42.0;
  }

  struct small_aggregate {
    long x;
    long y;
  };
  static small_aggregate small_aggregate_return() {
    return {32, 64};
  }

  struct large_aggregate {
    long x;
    long y;
    long z;
  };
  static large_aggregate indirect_return() {
    return {7, 8, 6};
  }

  // Maximally-sized homogenous floating-point aggregate (HFA).
  struct hfa {
    double a;
    double b;
    double c;
    double d;
  };
  static hfa hfa_return() {
    return {1.2, 3.4, 5.6, 7.8};
  }

  static int integer_args(int a, int b) {
    return a + b;
  }

  static double float_args(double a, double b) {
    return a - b;
  }

  static long lots_of_args(
      int a,
      long b,
      small_aggregate c,
      small_aggregate d,
      large_aggregate e,
      short f,
      int g,
      long h,
      float i,
      float j,
      hfa k,
      double l,
      double m,
      hfa n) {
    return a + b + c.x + c.y + d.x + d.y + e.x + e.y + e.z + f + g + h + i + j +
        k.a + k.b + k.c + k.d + l + m + n.a + n.b + n.c + n.d;
  }

  static void* test_hook_ptr;
};

void* TrampolineAbiTest::test_hook_ptr;

TEST_F(TrampolineAbiTest, testIntegerReturn) {
  test_hook_ptr = reinterpret_cast<void*>(integer_return);
  auto trampoline_code = reinterpret_cast<decltype(integer_return)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code();
  EXPECT_EQ(42, trampoline_return) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testFloatReturn) {
  test_hook_ptr = reinterpret_cast<void*>(float_return);
  auto trampoline_code = reinterpret_cast<decltype(float_return)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code();
  EXPECT_EQ(42.0, trampoline_return) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testSmallAggregateReturn) {
  test_hook_ptr = reinterpret_cast<void*>(small_aggregate_return);
  auto trampoline_code = reinterpret_cast<decltype(small_aggregate_return)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code();
  EXPECT_EQ(32, trampoline_return.x) << "return result not the same";
  EXPECT_EQ(64, trampoline_return.y) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testIndirectReturn) {
  test_hook_ptr = reinterpret_cast<void*>(indirect_return);
  auto trampoline_code = reinterpret_cast<decltype(indirect_return)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code();
  EXPECT_EQ(7, trampoline_return.x) << "return result not the same";
  EXPECT_EQ(8, trampoline_return.y) << "return result not the same";
  EXPECT_EQ(6, trampoline_return.z) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testHfaReturn) {
  test_hook_ptr = reinterpret_cast<void*>(hfa_return);
  auto trampoline_code = reinterpret_cast<decltype(hfa_return)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code();
  EXPECT_EQ(1.2, trampoline_return.a) << "return result not the same";
  EXPECT_EQ(3.4, trampoline_return.b) << "return result not the same";
  EXPECT_EQ(5.6, trampoline_return.c) << "return result not the same";
  EXPECT_EQ(7.8, trampoline_return.d) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testIntegerArgs) {
  test_hook_ptr = reinterpret_cast<void*>(integer_args);
  auto trampoline_code = reinterpret_cast<decltype(integer_args)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code(24, 7);
  EXPECT_EQ(31, trampoline_return) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testFloatArgs) {
  test_hook_ptr = reinterpret_cast<void*>(float_args);
  auto trampoline_code = reinterpret_cast<decltype(float_args)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code(2.5, 3.0);
  EXPECT_EQ(-0.5, trampoline_return) << "return result not the same";
}

TEST_F(TrampolineAbiTest, testLotsOfArgs) {
  test_hook_ptr = reinterpret_cast<void*>(lots_of_args);
  auto trampoline_code = reinterpret_cast<decltype(lots_of_args)*>(
      trampoline_template_pointer());

  auto trampoline_return = trampoline_code(
      1,
      2,
      {4, 8},
      {16, 32},
      {64, 128, 256},
      512,
      1024,
      2048,
      4096,
      8192,
      {16384, 32768, 65536, 131072},
      262144,
      524288,
      {1048576, 2097152, 4194304, 8388608});
  EXPECT_EQ(16777215, trampoline_return) << "return result not the same";
}

#endif

} // namespace trampoline
} // namespace plthooks
} // namespace facebook
