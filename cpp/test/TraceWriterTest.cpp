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

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>

#include <folly/experimental/TestUtil.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <zlib.h>
#include <zstr/zstr.hpp>

#include <profilo/LogEntry.h>
#include <profilo/PacketLogger.h>
#include <profilo/RingBuffer.h>
#include <profilo/entries/Entry.h>
#include <profilo/entries/EntryType.h>
#include <profilo/writer/TraceCallbacks.h>
#include <profilo/writer/TraceWriter.h>

using namespace facebook::profilo::logger;
using namespace facebook::profilo::writer;
using namespace facebook::profilo::entries;
namespace test = folly::test;
namespace fs = boost::filesystem;

namespace facebook {
namespace profilo {

const int64_t kTraceID = 1;
const int64_t kSecondTraceID = 2;
const char* kTraceIDString = "AAAAAAAAAAB";
const char* kTracePrefix = "test-prefix";

class MockCallbacks : public TraceCallbacks {
 public:
  MOCK_METHOD3(onTraceStart, void(int64_t, int32_t, std::string));
  MOCK_METHOD2(onTraceEnd, void(int64_t, uint32_t));
  MOCK_METHOD2(onTraceAbort, void(int64_t, AbortReason));
};

class TraceWriterTest : public ::testing::Test {
 protected:
  const size_t kBufferSize = 5;

  static std::vector<std::pair<std::string, std::string>> generateHeaders() {
    auto headers = std::vector<std::pair<std::string, std::string>>();
    headers.push_back(std::make_pair("key1", "value1"));
    headers.push_back(std::make_pair("key2", "value2"));
    return headers;
  }

  TraceWriterTest()
      : ::testing::Test(),
        trace_dir_("trace-folder-"),
        buffer_(kBufferSize),
        logger_([this]() -> PacketBuffer& { return this->buffer_; }),
        callbacks_(std::make_shared<::testing::NiceMock<MockCallbacks>>()),
        writer_(
            std::move(trace_dir_.path().generic_string()),
            "test-prefix",
            buffer_,
            callbacks_,
            generateHeaders()) {}

  test::TemporaryDirectory trace_dir_;
  TraceBuffer buffer_;
  PacketLogger logger_;
  std::shared_ptr<::testing::NiceMock<MockCallbacks>> callbacks_;
  TraceWriter writer_;

  void writeTraceStart(int64_t trace_id = kTraceID) {
    char payload[sizeof(StandardEntry) + 1]{};
    StandardEntry start{
        .id = 1,
        .type = entries::TRACE_START,
        .timestamp = 123,
        .tid = 0,
        .callid = 0,
        .matchid = 0,
        .extra = trace_id,
    };
    StandardEntry::pack(start, payload, sizeof(payload));
    logger_.write(payload, sizeof(payload));
  }

  void writeTraceEnd(int64_t trace_id = kTraceID) {
    char payload[sizeof(StandardEntry) + 1]{};
    StandardEntry start{
        .id = 2,
        .type = entries::TRACE_END,
        .timestamp = 124,
        .tid = 0,
        .callid = 0,
        .matchid = 0,
        .extra = trace_id,
    };
    StandardEntry::pack(start, payload, sizeof(payload));
    logger_.write(payload, sizeof(payload));
  }

  void writeTraceAbort(int64_t trace_id = kTraceID) {
    char payload[sizeof(StandardEntry) + 1]{};
    StandardEntry start{
        .id = 2,
        .type = entries::TRACE_ABORT,
        .timestamp = 125,
        .tid = 0,
        .callid = 0,
        .matchid = 0,
        .extra = trace_id,
    };
    StandardEntry::pack(start, payload, sizeof(payload));
    logger_.write(payload, sizeof(payload));
  }

  void writeFillerEvent() {
    char payload[sizeof(StandardEntry) + 1]{};
    StandardEntry start{
        .id = 2,
        .type = entries::MARK_PUSH,
        .timestamp = 125,
        .tid = 0,
        .callid = 0,
        .matchid = 0,
        .extra = 0,
    };
    StandardEntry::pack(start, payload, sizeof(payload));
    logger_.write(payload, sizeof(payload));
  }

  size_t getFileCount() {
    auto dir_iter = fs::recursive_directory_iterator(trace_dir_.path());
    auto is_file = [](const fs::directory_entry& x) {
      return fs::is_regular_file(x.path());
    };

    auto file_count =
        std::count_if(dir_iter, fs::recursive_directory_iterator(), is_file);
    return file_count;
  }

  fs::path getOnlyTraceFile() {
    EXPECT_EQ(getFileCount(), 1);

    auto dir_iter = fs::recursive_directory_iterator(trace_dir_.path());
    auto is_file = [](const fs::directory_entry& x) {
      return fs::is_regular_file(x.path());
    };
    auto file = (*std::find_if(
                     fs::recursive_directory_iterator(trace_dir_.path()),
                     fs::recursive_directory_iterator(),
                     is_file))
                    .path();

    return file;
  }

  std::string getOnlyTraceFileContents() {
    auto file = getOnlyTraceFile();

    std::stringstream output;
    zstr::ifstream input(file.generic_string());
    output << input.rdbuf();
    return output.str();
  }

  // Helper functions to simplify repetitive test cases
  void testNoTraceStartCursorAtTail(std::function<void()> end_event_fn);
  void testCallbackCalls(std::function<void()> expectations);
};

TEST_F(TraceWriterTest, testLoopStop) {
  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(buffer_.currentTail(), TraceWriter::kStopLoopTraceID);
  thread.join();
}

TEST_F(TraceWriterTest, testTraceFileCreatedSimple) {
  writeTraceStart();
  writeTraceEnd();

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();

  EXPECT_EQ(getFileCount(), 1) << "There should be only one real file.";

  auto file = getOnlyTraceFile();
  auto folder_name = file.parent_path().filename().generic_string();
  EXPECT_EQ(folder_name, kTraceIDString)
      << "Containing folder must be called " << kTraceIDString;

  auto filename = file.filename().generic_string();
  auto expected = std::string("-") + kTraceIDString + ".";
  EXPECT_NE(filename.find(expected), std::string::npos)
      << "Filename " << filename << " does not contain correct trace ID";

  EXPECT_EQ(filename.find(kTracePrefix), 0)
      << "Filename " << filename << " does not start with prefix";

  std::stringstream pid_str;
  pid_str << "-" << getpid() << "-";
  EXPECT_NE(filename.find(pid_str.str()), std::string::npos)
      << "Filename " << filename << " does not contain pid";
}

TEST_F(TraceWriterTest, testNoTraceSubmitPastStart) {
  writeTraceStart();
  auto cursorPastStart = buffer_.currentHead();
  writeTraceEnd();

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(cursorPastStart, kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();

  EXPECT_EQ(getFileCount(), 0);
}

TEST_F(TraceWriterTest, testNoTraceSubmitCursorOutOfBounds) {
  writeTraceStart();
  auto cursorAtTraceStart = buffer_.currentTail();

  // force a wrap around
  for (int i = 0; i < kBufferSize; ++i) {
    writeTraceEnd();
  }

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(cursorAtTraceStart, kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();

  EXPECT_EQ(getFileCount(), 0);
}

void TraceWriterTest::testNoTraceStartCursorAtTail(
    std::function<void()> end_event_fn) {
  auto cursorAtBeginning = buffer_.currentTail();

  end_event_fn();

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(cursorAtBeginning, kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();

  //
  // If we got here, it means that the writer did not block waiting for
  // the next entry after seeing an end event for our trace ID.
  //
  // Instead, it handled our cancel request correctly.
  //

  EXPECT_EQ(getFileCount(), 0);
}

TEST_F(TraceWriterTest, testNoTraceStartCursorAtTailWithTraceEnd) {
  testNoTraceStartCursorAtTail([&] { writeTraceEnd(); });
}

TEST_F(TraceWriterTest, testNoTraceStartCursorAtTailWithTraceAbort) {
  testNoTraceStartCursorAtTail([&] { writeTraceAbort(); });
}

TEST_F(TraceWriterTest, testHeadersPropagateToFile) {
  writeTraceStart();
  writeTraceEnd();

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();

  auto trace = getOnlyTraceFileContents();

  EXPECT_NE(trace.find("key1|value1"), std::string::npos);
  EXPECT_NE(trace.find("key2|value2"), std::string::npos);
}

void TraceWriterTest::testCallbackCalls(std::function<void()> expectations) {
  ::testing::InSequence dummy_;

  auto buffer_start = buffer_.currentHead();

  expectations();

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(buffer_start, kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();
}

TEST_F(TraceWriterTest, testCallbacksInOrderSuccess) {
  using ::testing::_;
  testCallbackCalls([&] {
    EXPECT_CALL(*callbacks_, onTraceStart(kTraceID, 0, _));
    EXPECT_CALL(*callbacks_, onTraceEnd(kTraceID, _));
    EXPECT_CALL(*callbacks_, onTraceAbort(_, _)).Times(0);

    writeTraceStart();
    writeTraceEnd();
  });
}

TEST_F(TraceWriterTest, testCallbacksInOrderAbort) {
  using ::testing::_;
  testCallbackCalls([&] {
    EXPECT_CALL(*callbacks_, onTraceStart(kTraceID, 0, _));
    EXPECT_CALL(*callbacks_, onTraceEnd(_, _)).Times(0);
    EXPECT_CALL(
        *callbacks_, onTraceAbort(kTraceID, AbortReason::CONTROLLER_INITIATED));

    writeTraceStart();
    writeTraceAbort();
  });
}

TEST_F(TraceWriterTest, testCallbacksMissedStart) {
  using ::testing::_;
  testCallbackCalls([&] {
    EXPECT_CALL(*callbacks_, onTraceStart(_, _, _)).Times(0);
    EXPECT_CALL(*callbacks_, onTraceEnd(_, _)).Times(0);
    EXPECT_CALL(*callbacks_, onTraceAbort(kTraceID, _)).Times(0);

    writeTraceStart();
    for (int idx = 0; idx < kBufferSize; idx++) {
      writeFillerEvent();
    }
  });
}

TEST_F(TraceWriterTest, testCallbacksSuccessMultiTracing) {
  using ::testing::_;

  auto buffer_start = buffer_.currentHead();

  EXPECT_CALL(*callbacks_, onTraceStart(kTraceID, _, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceStart(kSecondTraceID, _, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceEnd(kTraceID, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceEnd(kSecondTraceID, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceAbort(_, _)).Times(0);

  auto thread = std::thread([&] { writer_.loop(); });

  writeTraceStart(kTraceID);
  writer_.submit(buffer_start, kTraceID);
  buffer_start = buffer_.currentHead();
  writeTraceStart(kSecondTraceID);
  writer_.submit(buffer_start, kSecondTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);

  writeTraceEnd(kTraceID);
  writeTraceEnd(kSecondTraceID);

  thread.join();
}

TEST_F(TraceWriterTest, testCallbacksSuccessMultiTracing2) {
  using ::testing::_;

  auto buffer_start = buffer_.currentHead();

  EXPECT_CALL(*callbacks_, onTraceStart(kTraceID, _, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceStart(kSecondTraceID, _, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceEnd(kTraceID, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceEnd(kSecondTraceID, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceAbort(_, _)).Times(0);

  auto thread = std::thread([&] { writer_.loop(); });

  writeTraceStart(kTraceID);
  writeTraceEnd(kTraceID);
  writer_.submit(buffer_start, kTraceID);
  buffer_start = buffer_.currentHead();
  writeTraceStart(kSecondTraceID);
  writeTraceEnd(kSecondTraceID);
  writer_.submit(buffer_start, kSecondTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);

  thread.join();
}

TEST_F(TraceWriterTest, testCallbacksMultiTracingAbort) {
  using ::testing::_;

  auto buffer_start = buffer_.currentHead();

  EXPECT_CALL(*callbacks_, onTraceStart(kTraceID, _, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceStart(kSecondTraceID, _, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceEnd(kTraceID, _)).Times(0);
  EXPECT_CALL(*callbacks_, onTraceEnd(kSecondTraceID, _)).Times(1);
  EXPECT_CALL(*callbacks_, onTraceAbort(kSecondTraceID, _)).Times(0);
  EXPECT_CALL(*callbacks_, onTraceAbort(kTraceID, _)).Times(1);

  auto thread = std::thread([&] { writer_.loop(); });

  writeTraceStart(kTraceID);
  writer_.submit(buffer_start, kTraceID);
  buffer_start = buffer_.currentHead();
  writeTraceStart(kSecondTraceID);
  writeTraceAbort(kTraceID);
  writeTraceEnd(kSecondTraceID);
  writer_.submit(buffer_start, kSecondTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);

  thread.join();
}

TEST_F(TraceWriterTest, testTraceCRC32Checksum) {
  using ::testing::_;
  writeTraceStart();
  writeTraceEnd();

  uint32_t crc;
  EXPECT_CALL(*callbacks_, onTraceEnd(_, _))
      .WillOnce(testing::SaveArg<1>(&crc));

  auto thread = std::thread([&] { writer_.loop(); });

  writer_.submit(kTraceID);
  writer_.submit(TraceWriter::kStopLoopTraceID);
  thread.join();

  auto trace = getOnlyTraceFileContents();
  auto result_crc = crc32(
      0,
      reinterpret_cast<const unsigned char*>(trace.c_str()),
      strlen(trace.c_str()));
  EXPECT_EQ(result_crc, crc);
}

} // namespace profilo
} // namespace facebook
