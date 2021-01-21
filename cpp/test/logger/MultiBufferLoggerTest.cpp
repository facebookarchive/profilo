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
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <profilo/MultiBufferLogger.h>
#include <profilo/mmapbuf/Buffer.h>

namespace facebook {
namespace profilo {
namespace logger {

using mmapbuf::Buffer;

template <typename T>
void readOneEntry(T& result, TraceBuffer& buffer, TraceBuffer::Cursor cursor) {
  Packet packet{};
  ASSERT_TRUE(buffer.tryRead(packet, cursor));
  T::unpack(result, packet.data, packet.size);
}

TEST(MultiBufferLoggerTest, testMultiBufferWrite) {
  MultiBufferLogger logger{};

  auto buffer1 = std::make_shared<Buffer>(10);
  auto buffer2 = std::make_shared<Buffer>(10);
  logger.addBuffer(buffer1);
  logger.addBuffer(buffer2);

  StandardEntry entry{
      .id = 0,
      .type = EntryType::TRACE_START,
      .timestamp = 100,
      .tid = 1,
      .callid = 200,
      .matchid = 300,
      .extra = 400,
  };
  logger.write(entry);

  StandardEntry result1{}, result2{};
  readOneEntry(
      result1, buffer1->ringBuffer(), buffer1->ringBuffer().currentTail());
  readOneEntry(
      result2, buffer2->ringBuffer(), buffer2->ringBuffer().currentTail());

  EXPECT_NE(entry.id, 0);
  EXPECT_EQ(entry.id, result1.id);
  EXPECT_EQ(entry.id, result2.id);
  EXPECT_EQ(entry.type, result1.type);
  EXPECT_EQ(entry.type, result2.type);
  EXPECT_EQ(entry.timestamp, result1.timestamp);
  EXPECT_EQ(entry.timestamp, result2.timestamp);
  EXPECT_EQ(entry.tid, result1.tid);
  EXPECT_EQ(entry.tid, result2.tid);
  EXPECT_EQ(entry.callid, result1.callid);
  EXPECT_EQ(entry.callid, result2.callid);
  EXPECT_EQ(entry.matchid, result1.matchid);
  EXPECT_EQ(entry.matchid, result2.matchid);
  EXPECT_EQ(entry.extra, result1.extra);
  EXPECT_EQ(entry.extra, result2.extra);
}

} // namespace logger
} // namespace profilo
} // namespace facebook
