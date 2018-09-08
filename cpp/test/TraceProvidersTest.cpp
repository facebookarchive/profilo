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

#include <limits>

#include <gtest/gtest.h>
#include <profilo/TraceProviders.h>

namespace facebook {
namespace profilo {

TEST(TraceProviders, testAdd) {
  auto& tp = TraceProviders::get();

  tp.clearAllProviders();
  tp.enableProviders(0b0101);

  EXPECT_TRUE(tp.isEnabled(0b0001));
  EXPECT_TRUE(tp.isEnabled(0b0100));
  EXPECT_TRUE(tp.isEnabled(0b0101));
  EXPECT_FALSE(tp.isEnabled(0b0010));
  EXPECT_FALSE(tp.isEnabled(0b1000));
  EXPECT_FALSE(tp.isEnabled(0b1010));
}

TEST(TraceProviders, testRemove) {
  auto& tp = TraceProviders::get();

  tp.clearAllProviders();
  tp.enableProviders(0b1111);
  tp.disableProviders(0b0100);

  EXPECT_TRUE(tp.isEnabled(0b1000));
  EXPECT_FALSE(tp.isEnabled(0b0100));
  EXPECT_TRUE(tp.isEnabled(0b0010));
  EXPECT_TRUE(tp.isEnabled(0b0001));
}

TEST(TraceProviders, testAddRemoveEachBit) {
  auto& tp = TraceProviders::get();
  tp.clearAllProviders();

  uint32_t mask = 0;
  for (int i = 0; i < 32; i++) {
    int bit = 1 << i;
    tp.enableProviders(bit);
    mask |= bit;
    EXPECT_TRUE(tp.isEnabled(mask));
  }

  EXPECT_TRUE(tp.isEnabled(std::numeric_limits<uint32_t>::max()));

  for (int i = 0; i < 32; i++) {
    int bit = 1 << i;
    tp.disableProviders(bit);
    mask &= ~bit;

    EXPECT_FALSE(tp.isEnabled(bit));
    EXPECT_TRUE(tp.isEnabled(mask));
  }
}

TEST(TraceProviders, testAddRemoveForMultipleTraces) {
  auto& tp = TraceProviders::get();
  tp.clearAllProviders();
  int providers = 0b0101;
  tp.enableProviders(providers);
  tp.enableProviders(providers);

  EXPECT_TRUE(tp.isEnabled(providers));
  tp.disableProviders(providers);
  EXPECT_TRUE(tp.isEnabled(providers));
  tp.disableProviders(providers);
  EXPECT_FALSE(tp.isEnabled(providers));
  tp.disableProviders(providers);
  EXPECT_FALSE(tp.isEnabled(providers));
  tp.enableProviders(providers);
  EXPECT_TRUE(tp.isEnabled(providers));
}

TEST(TraceProviders, testClearAllProviders) {
  auto& tp = TraceProviders::get();
  tp.clearAllProviders();
  tp.enableProviders(0b0101);
  tp.enableProviders(0b1000);
  tp.clearAllProviders();

  EXPECT_FALSE(tp.isEnabled(0b0001));
  EXPECT_FALSE(tp.isEnabled(0b0010));
  EXPECT_FALSE(tp.isEnabled(0b0100));
  EXPECT_FALSE(tp.isEnabled(0b1000));
}

} // namespace profilo
} // namespace facebook
