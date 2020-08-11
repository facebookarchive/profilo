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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <counters/Counter.h>
#include <vector>

namespace facebook {
namespace profilo {
namespace counters {

constexpr auto kTid = 12345;
constexpr auto kCounterType = QuickLogConstants::THREAD_CPU_TIME;
constexpr auto kValueA = 22;
constexpr auto kValueB = 44;
constexpr auto kTimestamp1 = 1;
constexpr auto kTimestamp2 = 2;
constexpr auto kTimestamp3 = 3;
constexpr auto kTimestamp4 = 4;

using namespace ::testing;

class MockLogger {
 public:
  MOCK_METHOD1(write, int32_t(StandardEntry&&));

  MockLogger() {
    ON_CALL(*this, write(_))
        .WillByDefault(Invoke([this](StandardEntry&& entry) {
          recordedEntries.push_back(std::move(entry));
          return 0;
        }));
  }

  std::vector<StandardEntry> recordedEntries;
};

class TracedCounterTest : public Test {
 protected:
  TracedCounterTest()
      : mockLogger_(), counter_(&mockLogger_, kCounterType, kTid) {}

  MockLogger mockLogger_;
  Counter<MockLogger> counter_;
};

TEST_F(TracedCounterTest, testTimestampInvariantIsProtected) {
  counter_.record(kValueA, kTimestamp1);
  EXPECT_THROW(counter_.record(kValueB, kTimestamp1), std::runtime_error);
}

TEST_F(TracedCounterTest, testSinglePointLoggingCorrectness) {
  EXPECT_CALL(mockLogger_, write(_)).Times(1);
  counter_.record(kValueA, kTimestamp1);
  StandardEntry loggedEntry = mockLogger_.recordedEntries.front();
  EXPECT_EQ(loggedEntry.type, EntryType::COUNTER);
  EXPECT_EQ(loggedEntry.timestamp, kTimestamp1);
  EXPECT_EQ(loggedEntry.tid, kTid);
  EXPECT_EQ(loggedEntry.callid, kCounterType);
  EXPECT_EQ(loggedEntry.extra, kValueA);
}

TEST_F(TracedCounterTest, testZeroInitalCounterValueIsLogged) {
  EXPECT_CALL(mockLogger_, write(_)).Times(3);
  counter_.record(0, kTimestamp1);
  counter_.record(0, kTimestamp2);
  counter_.record(kValueA, kTimestamp3);
}

//
// * --- x
//
// [*] - logged point
// [x] - skipped point
//
TEST_F(TracedCounterTest, testDuplicatePointsAreIgnored) {
  EXPECT_CALL(mockLogger_, write(_)).Times(1);
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueA, kTimestamp2);
}

//
//      *
//    /
//  *
//
TEST_F(TracedCounterTest, testMovingAdjacentValuesAreLogged) {
  EXPECT_CALL(mockLogger_, write(_)).Times(2);
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueB, kTimestamp2);
  StandardEntry aEntry = mockLogger_.recordedEntries.front();
  EXPECT_EQ(aEntry.timestamp, kTimestamp1);
  EXPECT_EQ(aEntry.extra, kValueA);
  StandardEntry bEntry = mockLogger_.recordedEntries.back();
  EXPECT_EQ(bEntry.timestamp, kTimestamp2);
  EXPECT_EQ(bEntry.extra, kValueB);
}

//
//            *
//          /
//  * --- *
//
TEST_F(TracedCounterTest, testThreePointsWithOneDuplicate) {
  EXPECT_CALL(mockLogger_, write(_)).Times(3);
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueA, kTimestamp2);
  counter_.record(kValueB, kTimestamp3);
  StandardEntry aEntry = mockLogger_.recordedEntries.at(0);
  EXPECT_EQ(aEntry.timestamp, kTimestamp1);
  EXPECT_EQ(aEntry.extra, kValueA);
  StandardEntry bEntry = mockLogger_.recordedEntries.at(1);
  EXPECT_EQ(bEntry.timestamp, kTimestamp2);
  EXPECT_EQ(bEntry.extra, kValueA);
  StandardEntry cEntry = mockLogger_.recordedEntries.at(2);
  EXPECT_EQ(cEntry.timestamp, kTimestamp3);
  EXPECT_EQ(cEntry.extra, kValueB);
}

//
//                  *
//                /
//  * --- x --- *
//
TEST_F(TracedCounterTest, testFourPointsWithOneDuplicate) {
  EXPECT_CALL(mockLogger_, write(_)).Times(3);
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueA, kTimestamp2);
  counter_.record(kValueA, kTimestamp3);
  counter_.record(kValueB, kTimestamp4);
  StandardEntry aEntry = mockLogger_.recordedEntries.at(0);
  EXPECT_EQ(aEntry.timestamp, kTimestamp1);
  EXPECT_EQ(aEntry.extra, kValueA);
  StandardEntry bEntry = mockLogger_.recordedEntries.at(1);
  EXPECT_EQ(bEntry.timestamp, kTimestamp3);
  EXPECT_EQ(bEntry.extra, kValueA);
  StandardEntry cEntry = mockLogger_.recordedEntries.at(2);
  EXPECT_EQ(cEntry.timestamp, kTimestamp4);
  EXPECT_EQ(cEntry.extra, kValueB);
}

} // namespace counters
} // namespace profilo
} // namespace facebook
