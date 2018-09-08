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

#include <profilo/entries/Entry.h>
#include <profilo/entries/EntryParser.h>
#include <profilo/entries/EntryType.h>
#include <profilo/writer/PrintEntryVisitor.h>

namespace facebook {
namespace profilo {
namespace entries {

using namespace writer;

class TestVisitor : public EntryVisitor {
 public:
  StandardEntry standardEntry;
  FramesEntry framesEntry;
  BytesEntry bytesEntry;

  virtual void visit(const StandardEntry& entry) {
    standardEntry = entry;
  }
  virtual void visit(const FramesEntry& entry) {
    framesEntry = entry;
  }
  virtual void visit(const BytesEntry& entry) {
    bytesEntry = entry;
  }
};

TEST(EntryCodegen, testPackUnpackStandardEntry) {
  StandardEntry input{.id = 10,
                      .type = TRACE_START,
                      .timestamp = 123,
                      .tid = 0,
                      .callid = 1,
                      .matchid = 2,
                      .extra = 3};

  char buffer[sizeof(input) * 2]{};
  StandardEntry::pack(input, buffer, sizeof(buffer));

  TestVisitor visitor;
  EntryParser::parse(buffer, sizeof(buffer), visitor);

  auto& entry = visitor.standardEntry;
  EXPECT_EQ(input.id, entry.id);
  EXPECT_EQ(input.type, entry.type);
  EXPECT_EQ(input.timestamp, entry.timestamp);
  EXPECT_EQ(input.tid, entry.tid);
  EXPECT_EQ(input.callid, entry.callid);
  EXPECT_EQ(input.matchid, entry.matchid);
  EXPECT_EQ(input.extra, entry.extra);
}

TEST(EntryCodegen, testPrintStandardEntry) {
  StandardEntry input{.id = 10,
                      .type = TRACE_START,
                      .timestamp = 123,
                      .tid = 0,
                      .callid = 1,
                      .matchid = 2,
                      .extra = 3};

  char buffer[sizeof(StandardEntry) * 2]{};
  StandardEntry::pack(input, buffer, sizeof(buffer));

  std::stringstream stream;
  PrintEntryVisitor visitor(stream);
  EntryParser::parse(buffer, sizeof(buffer), visitor);

  EXPECT_EQ(stream.str(), "10|TRACE_START|123|0|1|2|3\n");
}

TEST(EntryCodegen, testPackUnpackBytesEntry) {
  uint8_t bytes[] = {'h', 'i', '!'};
  BytesEntry input{
      .id = 10,
      .type = STRING_KEY,
      .matchid = 1,
      .bytes.values = bytes,
      .bytes.size = 3,
  };

  char buffer[sizeof(input) * 2]{};
  BytesEntry::pack(input, buffer, sizeof(buffer));

  TestVisitor visitor;
  EntryParser::parse(buffer, sizeof(buffer), visitor);

  auto& entry = visitor.bytesEntry;
  EXPECT_EQ(input.id, entry.id);
  EXPECT_EQ(input.type, entry.type);
  EXPECT_EQ(input.matchid, entry.matchid);
  EXPECT_EQ(input.bytes.size, entry.bytes.size);
  EXPECT_TRUE(std::equal(
      input.bytes.values,
      input.bytes.values + input.bytes.size,
      entry.bytes.values));
}

TEST(EntryCodegen, testPrintBytesEntry) {
  uint8_t bytes[] = {'h', 'i', '!'};
  BytesEntry input{
      .id = 10,
      .type = STRING_KEY,
      .matchid = 1,
      .bytes.values = bytes,
      .bytes.size = 3,
  };

  char buffer[sizeof(input) * 2]{};
  BytesEntry::pack(input, buffer, sizeof(buffer));

  std::stringstream stream;
  PrintEntryVisitor visitor(stream);
  EntryParser::parse(buffer, sizeof(buffer), visitor);

  EXPECT_EQ(stream.str(), "10|STRING_KEY|1|hi!\n");
}

TEST(EntryCodegen, testPackUnpackFramesEntry) {
  int64_t frames[] = {100, 200, 300};
  FramesEntry input{
      .id = 10,
      .type = STACK_FRAME,
      .timestamp = 123,
      .tid = 1,
      .frames.values = frames,
      .frames.size = 3,
  };

  char buffer[sizeof(input) * 2]{};
  FramesEntry::pack(input, buffer, sizeof(buffer));

  TestVisitor visitor;
  EntryParser::parse(buffer, sizeof(buffer), visitor);

  auto& entry = visitor.framesEntry;
  EXPECT_EQ(input.id, entry.id);
  EXPECT_EQ(input.type, entry.type);
  EXPECT_EQ(input.timestamp, entry.timestamp);
  EXPECT_EQ(input.tid, entry.tid);
  EXPECT_EQ(input.frames.size, entry.frames.size);
  EXPECT_TRUE(std::equal(
      input.frames.values,
      input.frames.values + input.frames.size,
      entry.frames.values));
}

TEST(EntryCodegen, testPrintFramesEntry) {
  int64_t frames[] = {100, 200, 300};
  FramesEntry input{
      .id = 10,
      .type = STACK_FRAME,
      .timestamp = 123,
      .tid = 1,
      .frames.values = frames,
      .frames.size = 3,
  };

  char buffer[sizeof(input) * 2]{};
  FramesEntry::pack(input, buffer, sizeof(buffer));

  std::stringstream stream;
  PrintEntryVisitor visitor(stream);
  EntryParser::parse(buffer, sizeof(buffer), visitor);

  EXPECT_EQ(
      stream.str(),
      "10|STACK_FRAME|123|1|0|0|100\n"
      "10|STACK_FRAME|123|1|0|0|200\n"
      "10|STACK_FRAME|123|1|0|0|300\n");
}

TEST(EntryCodegen, testPackNullptrThrows) {
  StandardEntry standard{};
  BytesEntry bytes{};
  FramesEntry frames{};

  try {
    StandardEntry::pack(standard, nullptr, sizeof(StandardEntry) * 2);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }

  try {
    BytesEntry::pack(bytes, nullptr, sizeof(BytesEntry) * 2);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }

  try {
    FramesEntry::pack(frames, nullptr, sizeof(FramesEntry) * 2);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }
}

TEST(EntryCodegen, testUnpackNullptrThrows) {
  StandardEntry standard{};
  BytesEntry bytes{};
  FramesEntry frames{};

  try {
    StandardEntry::unpack(standard, nullptr, sizeof(StandardEntry) * 2);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }

  try {
    BytesEntry::unpack(bytes, nullptr, sizeof(BytesEntry) * 2);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }

  try {
    FramesEntry::unpack(frames, nullptr, sizeof(FramesEntry) * 2);
    FAIL() << "Expected std::invalid_argument";
  } catch (const std::invalid_argument& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::invalid_argument";
  }
}

TEST(EntryCodegen, testPackTooSmallThrows) {
  StandardEntry standard{};
  BytesEntry bytes{};
  FramesEntry frames{};

  char buffer[10];

  try {
    StandardEntry::pack(standard, buffer, sizeof(buffer));
    FAIL() << "Expected std::out_of_range";
  } catch (const std::out_of_range& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    char standard_buffer[sizeof(StandardEntry)]; // should be off by one
    StandardEntry::pack(standard, standard_buffer, sizeof(standard_buffer));
    FAIL() << "Expected std::out_of_range";
  } catch (const std::out_of_range& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    BytesEntry::pack(bytes, buffer, sizeof(buffer));
    FAIL() << "Expected std::out_of_range";
  } catch (const std::out_of_range& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }

  try {
    FramesEntry::pack(frames, buffer, sizeof(buffer));
    FAIL() << "Expected std::out_of_range";
  } catch (const std::out_of_range& ex) {
    // intentionally empty
  } catch (...) {
    FAIL() << "Expected std::out_of_range";
  }
}

} // namespace entries
} // namespace profilo
} // namespace facebook
