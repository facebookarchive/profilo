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
#include <sstream>

#include <gtest/gtest.h>

#include <profilo/entries/EntryParser.h>
#include <profilo/writer/DeltaEncodingVisitor.h>
#include <profilo/writer/PrintEntryVisitor.h>

using namespace facebook::profilo::entries;
using namespace facebook::profilo::writer;

namespace facebook {
namespace profilo {

TEST(DeltaEncodingVisitorTest, testDeltaEncodeStandardEntry) {
  std::stringstream stream;
  PrintEntryVisitor print(stream);
  DeltaEncodingVisitor delta(print);

  delta.visit(StandardEntry{.id = 10,
                            .type = TRACE_START,
                            .timestamp = 123,
                            .tid = 0,
                            .callid = 1,
                            .matchid = 2,
                            .extra = 3});

  delta.visit(StandardEntry{
      .id = 11,
      .type = TRACE_END,
      .timestamp = 124,
      .tid = 1,
      .callid = 2,
      .matchid = 3,
      .extra = 0,
  });

  EXPECT_EQ(
      stream.str(),
      "10|TRACE_START|123|0|1|2|3\n"
      "1|TRACE_END|1|1|1|1|-3\n");
}

TEST(DeltaEncodingVisitorTest, testDeltaEncodeStandardEntryIntegerOverflow) {
  std::stringstream stream;
  PrintEntryVisitor print(stream);
  DeltaEncodingVisitor delta(print);

  delta.visit(StandardEntry{
      .id = 10,
      .type = TRACE_START,
      .timestamp = 123,
      .tid = 0,
      .callid = 1,
      .matchid = 2,
      .extra = -10,
  });

  delta.visit(StandardEntry{
      .id = 11,
      .type = TRACE_END,
      .timestamp = 124,
      .tid = 1,
      .callid = 2,
      .matchid = 3,
      .extra = std::numeric_limits<int64_t>::max(),
  });

  EXPECT_EQ(
      stream.str(),
      "10|TRACE_START|123|0|1|2|-10\n"
      "1|TRACE_END|1|1|1|1|-9223372036854775799\n");
}

TEST(DeltaEncodingVisitorTest, testDeltaEncodeFramesEntry) {
  std::stringstream stream;
  PrintEntryVisitor print(stream);
  DeltaEncodingVisitor delta(print);

  int64_t frames[] = {1000, 4000, 2000};
  delta.visit(FramesEntry{
      .id = 10,
      .type = STACK_FRAME,
      .timestamp = 123,
      .tid = 0,
      .frames.values = frames,
      .frames.size = 3,
  });

  EXPECT_EQ(
      stream.str(),
      "10|STACK_FRAME|123|0|0|0|1000\n"
      "1|STACK_FRAME|0|0|0|0|3000\n"
      "1|STACK_FRAME|0|0|0|0|-2000\n");
}

TEST(DeltaEncodingVisitorTest, testDeltaEncodeMixedEntries) {
  std::stringstream stream;
  PrintEntryVisitor print(stream);
  DeltaEncodingVisitor delta(print);

  delta.visit(StandardEntry{.id = 10,
                            .type = QPL_START,
                            .timestamp = 123,
                            .tid = 0,
                            .callid = 65545, // 0xFFFF + 10
                            .matchid = 2,
                            .extra = 3});

  uint8_t key[] = {'k', 'e', 'y'};
  delta.visit(BytesEntry{
      .id = 11,
      .type = STRING_KEY,
      .matchid = 10,
      .bytes.values = key,
      .bytes.size = 3,
  });

  uint8_t value[] = {'v', 'a', 'l', 'u', 'e'};
  delta.visit(BytesEntry{
      .id = 12,
      .type = STRING_VALUE,
      .matchid = 11,
      .bytes.values = value,
      .bytes.size = 5,
  });

  delta.visit(StandardEntry{.id = 13,
                            .type = QPL_END,
                            .timestamp = 124,
                            .tid = 0,
                            .callid = 65545,
                            .matchid = 2,
                            .extra = 3});

  int64_t frames[] = {1000, 2000, 3000};
  delta.visit(FramesEntry{
      .id = 14,
      .type = STACK_FRAME,
      .timestamp = 125,
      .tid = 0,
      .frames.values = frames,
      .frames.size = 3,
  });

  EXPECT_EQ(
      stream.str(),
      "10|QPL_START|123|0|65545|2|3\n"
      "11|STRING_KEY|10|key\n"
      "12|STRING_VALUE|11|value\n"
      "3|QPL_END|1|0|0|0|0\n"
      "1|STACK_FRAME|1|0|0|0|997\n"
      "1|STACK_FRAME|0|0|0|0|1000\n"
      "1|STACK_FRAME|0|0|0|0|1000\n");
}

} // namespace profilo
} // namespace facebook
