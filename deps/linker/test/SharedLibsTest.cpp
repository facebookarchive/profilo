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
#include <memory>
#include <linker/sharedlibs.h>
#include <linkertests/test.h>

using namespace facebook::linker;

struct SharedLibsTest : public BaseTest {
  SharedLibsTest()
    : libtarget(loadLibrary("libtarget.so")),
      libtarget2(loadLibrary("libtarget2.so")) { }

  std::unique_ptr<LibraryHandle> libtarget;
  std::unique_ptr<LibraryHandle> libtarget2;
};

TEST_F(SharedLibsTest, testLookupTarget) {
  auto result = sharedLib("libtarget.so");
  ASSERT_TRUE(result.success) << "not found";
  ASSERT_TRUE(result.data);
}

TEST_F(SharedLibsTest, testLookupSecond) {
  auto result = sharedLib("libtarget2.so");
  ASSERT_TRUE(result.success) << "not found";
  ASSERT_TRUE(result.data);
}

TEST_F(SharedLibsTest, testNotSameLib) {
  auto result1 = sharedLib("libtarget.so");
  auto result2 = sharedLib("libtarget2.so");
  ASSERT_TRUE(result1.success && result2.success);
  ASSERT_NE(result1.data, result2.data);
}

TEST_F(SharedLibsTest, testBadSharedLibCallFails) {
  auto result = sharedLib("lkjlkjlkj");
  ASSERT_FALSE(result.success);
  ASSERT_FALSE(result.data);
}

TEST_F(SharedLibsTest, testStaleSharedLibCallFails) {
  libtarget.reset();
  auto result = sharedLib("libtarget.so");
  ASSERT_FALSE(result.success);
  ASSERT_FALSE(result.data);
}

TEST_F(SharedLibsTest, testStaleSharedLibDataIsFalse) {
  auto result = sharedLib("libtarget.so");
  ASSERT_TRUE(result.success);
  auto lib = result.data;
  ASSERT_TRUE(lib);

  libtarget.reset();

  ASSERT_FALSE(lib);
}
