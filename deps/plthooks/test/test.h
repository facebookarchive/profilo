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

#include <linker/linker.h>
#include <gtest/gtest.h>
#include <fb/Build.h>

namespace facebook { namespace linker {
  void clearSharedLibs(); // testing only
} } // namespace facebook::linker

struct BaseTest : public testing::Test {
  virtual void SetUp() {
    linker_set_enabled(true);
    ASSERT_EQ(0, linker_initialize());
  }

  ~BaseTest() {
    facebook::linker::clearSharedLibs();
  }
};

#define LIBDIR(lib) (lib)
