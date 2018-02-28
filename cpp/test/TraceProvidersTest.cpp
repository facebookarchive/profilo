// Copyright 2004-present Facebook. All Rights Reserved.

#include <limits>

#include <profilo/TraceProviders.h>
#include <gtest/gtest.h>

namespace facebook {
namespace profilo {

TEST(TraceProviders, testAdd) {
  auto& tp = TraceProviders::get();

  tp.clearAllProviders();
  tp.enableProviders(0b0101);

  EXPECT_TRUE( tp.isEnabled(0b0001));
  EXPECT_TRUE( tp.isEnabled(0b0100));
  EXPECT_TRUE( tp.isEnabled(0b0101));
  EXPECT_FALSE(tp.isEnabled(0b0010));
  EXPECT_FALSE(tp.isEnabled(0b1000));
  EXPECT_FALSE(tp.isEnabled(0b1010));
}

TEST(TraceProviders, testRemove) {
  auto& tp = TraceProviders::get();

  tp.clearAllProviders();
  tp.enableProviders(0b1111);
  tp.disableProviders(0b0100);

  EXPECT_TRUE( tp.isEnabled(0b1000));
  EXPECT_FALSE(tp.isEnabled(0b0100));
  EXPECT_TRUE( tp.isEnabled(0b0010));
  EXPECT_TRUE( tp.isEnabled(0b0001));
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
