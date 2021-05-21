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

#include <gtest/gtest.h>
#include <profilo/perfevents/detail/FileBackedMappingsList.h>

namespace facebook {
namespace profilo {

using FBML = perfevents::detail::FileBackedMappingsList;

TEST(FileBackedMappingsList, testKnownBadStrings) {
  EXPECT_TRUE(FBML::isAnonymous("/dev/ashmem/dalvik-LinearAlloc (deleted)"));
  EXPECT_TRUE(FBML::isAnonymous("[anon:linker_alloc_32]"));
  EXPECT_TRUE(FBML::isAnonymous("[stack:7945]"));
  EXPECT_TRUE(FBML::isAnonymous("[anon:thread signal stack]"));
  EXPECT_TRUE(FBML::isAnonymous("anon_inode:dmabuf"));
}

TEST(FileBackedMappingsList, testKnownGoodStrings) {
  EXPECT_FALSE(FBML::isAnonymous("/system/fonts/Roboto-Medium.ttf"));
  EXPECT_FALSE(FBML::isAnonymous("/system/lib/libbinder.so"));
}

} // namespace profilo
} // namespace facebook
