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

#include <profilo/MultiBufferLogger.h>
#include <profilo/counters/Counter.h>
#include <vector>

using facebook::profilo::logger::MultiBufferLogger;
using facebook::profilo::mmapbuf::Buffer;

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

class TracedCounterTest : public Test {
 protected:
  TracedCounterTest()
      : buffer(std::make_shared<Buffer>(100)),
        logger(),
        counter_(logger, kCounterType, kTid) {
    logger.addBuffer(buffer);
  }

  std::vector<StandardEntry> writtenEntries() {
    auto& rb = buffer->ringBuffer();
    auto cursor = rb.currentTail();

    std::vector<StandardEntry> entries{};

    logger::Packet packet{};
    while (rb.tryRead(packet, cursor)) {
      StandardEntry entry{};
      StandardEntry::unpack(entry, packet.data, packet.size);
      entries.emplace_back(entry);

      cursor.moveForward();
    }
    return entries;
  }

  std::shared_ptr<Buffer> buffer;
  MultiBufferLogger logger;
  Counter counter_;
};

TEST_F(TracedCounterTest, testTimestampInvariantIsProtected) {
  counter_.record(kValueA, kTimestamp1);
  EXPECT_THROW(counter_.record(kValueB, kTimestamp1), std::runtime_error);
}

TEST_F(TracedCounterTest, testSinglePointLoggingCorrectness) {
  counter_.record(kValueA, kTimestamp1);
  auto entries = writtenEntries();
  EXPECT_EQ(entries.size(), 1);
  StandardEntry& loggedEntry = entries.front();
  EXPECT_EQ(loggedEntry.type, EntryType::COUNTER);
  EXPECT_EQ(loggedEntry.timestamp, kTimestamp1);
  EXPECT_EQ(loggedEntry.tid, kTid);
  EXPECT_EQ(loggedEntry.callid, kCounterType);
  EXPECT_EQ(loggedEntry.extra, kValueA);
}

TEST_F(TracedCounterTest, testZeroInitalCounterValueIsLogged) {
  counter_.record(0, kTimestamp1);
  counter_.record(0, kTimestamp2);
  counter_.record(kValueA, kTimestamp3);
  EXPECT_EQ(writtenEntries().size(), 3);
}

//
// * --- x
//
// [*] - logged point
// [x] - skipped point
//
TEST_F(TracedCounterTest, testDuplicatePointsAreIgnored) {
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueA, kTimestamp2);
  EXPECT_EQ(writtenEntries().size(), 1);
}

//
//      *
//    /
//  *
//
TEST_F(TracedCounterTest, testMovingAdjacentValuesAreLogged) {
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueB, kTimestamp2);

  auto entries = writtenEntries();
  EXPECT_EQ(entries.size(), 2);
  StandardEntry& aEntry = entries.front();
  EXPECT_EQ(aEntry.timestamp, kTimestamp1);
  EXPECT_EQ(aEntry.extra, kValueA);
  StandardEntry& bEntry = entries.back();
  EXPECT_EQ(bEntry.timestamp, kTimestamp2);
  EXPECT_EQ(bEntry.extra, kValueB);
}

//
//            *
//          /
//  * --- *
//
TEST_F(TracedCounterTest, testThreePointsWithOneDuplicate) {
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueA, kTimestamp2);
  counter_.record(kValueB, kTimestamp3);

  auto entries = writtenEntries();
  EXPECT_EQ(entries.size(), 3);
  StandardEntry aEntry = entries.at(0);
  EXPECT_EQ(aEntry.timestamp, kTimestamp1);
  EXPECT_EQ(aEntry.extra, kValueA);
  StandardEntry bEntry = entries.at(1);
  EXPECT_EQ(bEntry.timestamp, kTimestamp2);
  EXPECT_EQ(bEntry.extra, kValueA);
  StandardEntry cEntry = entries.at(2);
  EXPECT_EQ(cEntry.timestamp, kTimestamp3);
  EXPECT_EQ(cEntry.extra, kValueB);
}

//
//                  *
//                /
//  * --- x --- *
//
TEST_F(TracedCounterTest, testFourPointsWithOneDuplicate) {
  counter_.record(kValueA, kTimestamp1);
  counter_.record(kValueA, kTimestamp2);
  counter_.record(kValueA, kTimestamp3);
  counter_.record(kValueB, kTimestamp4);

  auto entries = writtenEntries();
  EXPECT_EQ(entries.size(), 3);
  StandardEntry aEntry = entries.at(0);
  EXPECT_EQ(aEntry.timestamp, kTimestamp1);
  EXPECT_EQ(aEntry.extra, kValueA);
  StandardEntry bEntry = entries.at(1);
  EXPECT_EQ(bEntry.timestamp, kTimestamp3);
  EXPECT_EQ(bEntry.extra, kValueA);
  StandardEntry cEntry = entries.at(2);
  EXPECT_EQ(cEntry.timestamp, kTimestamp4);
  EXPECT_EQ(cEntry.extra, kValueB);
}

} // namespace counters
} // namespace profilo
} // namespace facebook
