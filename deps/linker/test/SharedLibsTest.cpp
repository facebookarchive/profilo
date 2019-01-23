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

#include <cppdistract/dso.h>

#include <linker/sharedlibs.h>

#include <linkertests/test.h>

using namespace facebook::linker;

struct SharedLibsTest : public BaseTest {
  SharedLibsTest()
    : libtarget(std::make_unique<facebook::cppdistract::dso>(LIBDIR("libtarget.so"))),
      libgnu(LIBDIR("libgnu.so")) { }

  std::unique_ptr<facebook::cppdistract::dso> libtarget;
  facebook::cppdistract::dso const libgnu;
};

TEST_F(SharedLibsTest, testLookupTarget) {
  ASSERT_TRUE(sharedLib("libtarget.so")) << "not found";
}

TEST_F(SharedLibsTest, testLookupSecond) {
  ASSERT_TRUE(sharedLib("libgnu.so"));
}

TEST_F(SharedLibsTest, testNotSameLib) {
  ASSERT_NE(
    sharedLib("libtarget.so"),
    sharedLib("libgnu.so"));
}

TEST_F(SharedLibsTest, testBadSharedLibCallThrows) {
  ASSERT_THROW(sharedLib("lkjlkjlkj"), std::out_of_range);
}

TEST_F(SharedLibsTest, testStaleSharedLibCallThrows) {
  libtarget.reset();
  ASSERT_THROW(sharedLib("libtarget.so"), std::out_of_range);
}

TEST_F(SharedLibsTest, testStaleSharedLibDataIsFalse) {
  auto lib = sharedLib("libtarget.so");
  ASSERT_TRUE(lib);

  libtarget.reset();

  ASSERT_FALSE(lib);
}
