/**
 * Copyright 2018-present, Facebook, Inc.
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

#include <util/common.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string>

class CommonTest : public ::testing::Test {
 public:
  // Create a temporary directory where we'll create some more stuff later
  void SetUp() {
    char path[32]{};
    strcpy(path, "/tmp/temp.XXXXXXXXXX");
    mCreatedDir = mkdtemp(path);

    ASSERT_EQ(dirCreated(path), 0);
  }

  // Clean up
  void TearDown() {
    rmdir((std::string{mCreatedDir} + "/cache/profilo/TRACE_ID").c_str());
    rmdir((std::string{mCreatedDir} + "/cache/profilo/TRACE_ID_2").c_str());
    rmdir((std::string{mCreatedDir} + "/cache/profilo").c_str());
    rmdir((std::string{mCreatedDir} + "/cache").c_str());
    rmdir(mCreatedDir.c_str());
  }

  int dirCreated(char const* path) {
    struct stat s;

    return stat(path, &s);
  }

 protected:
  std::string mCreatedDir;
};

TEST_F(CommonTest, mkdirsTest) {
  std::string createMe{mCreatedDir};

  // The whole path needs to be recursively created
  createMe += "/cache/profilo/TRACE_ID";
  facebook::profilo::mkdirs(createMe.c_str());
  EXPECT_EQ(dirCreated(createMe.c_str()), 0);

  // Create a directory for a new trace
  createMe += "_2";
  facebook::profilo::mkdirs(createMe.c_str());
  EXPECT_EQ(dirCreated(createMe.c_str()), 0);

  // Test a race condition where the directory already exists
  facebook::profilo::mkdirs(createMe.c_str());
  EXPECT_EQ(dirCreated(createMe.c_str()), 0);
}
