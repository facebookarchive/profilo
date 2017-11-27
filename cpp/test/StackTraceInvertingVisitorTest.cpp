// Copyright 2004-present Facebook. All Rights Reserved.

#include <sstream>
#include <limits>

#include <gtest/gtest.h>

#include <loom/entries/EntryParser.h>
#include <loom/writer/PrintEntryVisitor.h>
#include <loom/writer/StackTraceInvertingVisitor.h>

using namespace facebook::loom::entries;
using namespace facebook::loom::writer;

namespace facebook {
namespace loom {

TEST(StackInvertingVisitorTest, testStacksAreInverted) {
  std::stringstream stream;
  PrintEntryVisitor print(stream);
  StackTraceInvertingVisitor stack(print);

  int64_t frames[] = {300, 200, 100};
  stack.visit(FramesEntry {
    .id = 1,
    .type = entries::STACK_FRAME,
    .timestamp = 1,
    .tid = 1,
    .frames = {
      .values = frames,
      .size = 3,
    },
  });

  int64_t otherframes[] = {3000, 2000, 1000};
  stack.visit(FramesEntry {
    .id = 2,
    .type = entries::STACK_FRAME,
    .timestamp = 2,
    .tid = 2,
    .frames = {
      .values = otherframes,
      .size = 3,
    },
  });

  EXPECT_EQ(
    stream.str(),
    "1|STACK_FRAME|1|1|0|0|100\n"
    "1|STACK_FRAME|1|1|0|0|200\n"
    "1|STACK_FRAME|1|1|0|0|300\n"
    "2|STACK_FRAME|2|2|0|0|1000\n"
    "2|STACK_FRAME|2|2|0|0|2000\n"
    "2|STACK_FRAME|2|2|0|0|3000\n"
  );
}

} // namespace loom
} // namespace facebook
