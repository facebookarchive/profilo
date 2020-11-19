// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/experimental/TestUtil.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <zlib.h>
#include <zstr/zstr.hpp>
#include <algorithm>
#include <climits>
#include <fstream>
#include <ostream>
#include <sstream>
#include <vector>

#include <profilo/entries/Entry.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/MmapBufferManager.h>
#include <profilo/mmapbuf/header/MmapBufferHeader.h>
#include <profilo/mmapbuf/writer/MmapBufferTraceWriter.h>
#include <profilo/writer/DeltaEncodingVisitor.h>
#include <profilo/writer/PrintEntryVisitor.h>
#include <profilo/writer/TimestampTruncatingVisitor.h>
#include <util/common.h>

using namespace facebook::profilo::logger;
using namespace facebook::profilo::entries;
using namespace facebook::profilo::mmapbuf::header;

namespace test = folly::test;
namespace fs = boost::filesystem;

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace writer {

constexpr char kTraceFolder[] = "mmabbuf-test-trace-folder";
constexpr char kTracePrefix[] = "mmabbuf-test-trace-";
constexpr int64_t kTraceId = 222;
constexpr int32_t kQplId = 33444;
constexpr int64_t kTraceRecollectionTimestamp = 1234567;
constexpr int32_t kConfigId = 11;
constexpr int32_t kBuildId = 17;

static const std::array<std::string, 3> kMappings = {
    "libhwui.so:722580c000:586015DEC7C4DA055D33796D9D793508:186000:491000",
    "libart-compiler.so:71987dd000:25CFFF6256F96F117E72005B5318E262:c2000:244000",
    "libc.so:7224896000:0965E88D999C749783C8947F9B7937E9:40000:a7000"};

#define EXPECT_STRING_CONTAINS(haystack, needle) \
  EXPECT_PRED_FORMAT2(assertContainsInString, haystack, needle)

static ::testing::AssertionResult assertContainsInString(
    const char* haystack_code_str,
    const char* needle_code_str,
    const std::string& haystack,
    const std::string& needle) {
  if (haystack.find(needle) == std::string::npos) {
    return ::testing::AssertionFailure()
        << needle_code_str << "=" << needle.c_str() << " is not found in "
        << haystack_code_str << ": " << haystack.c_str();
  }
  return ::testing::AssertionSuccess();
}

class MockCallbacks : public TraceCallbacks {
 public:
  MOCK_METHOD3(onTraceStart, void(int64_t, int32_t, std::string));
  MOCK_METHOD1(onTraceEnd, void(int64_t));
  MOCK_METHOD2(onTraceAbort, void(int64_t, AbortReason));
};

class MmapBufferTraceWriterTest : public ::testing::Test {
 protected:
  MmapBufferTraceWriterTest()
      : ::testing::Test(),
        loggedEntries_(),
        temp_dump_file_("test_dump"),
        temp_trace_folder_(kTraceFolder),
        temp_mappings_file_("maps", temp_dump_file_.path().parent_path()) {
    std::srand(std::time(nullptr));
  }

  const std::string& dumpPath() {
    return temp_dump_file_.path().generic_string();
  }

  const std::string& traceFolderPath() {
    return temp_trace_folder_.path().generic_string();
  }

  StandardEntry generateTraceBackwardsEntry() {
    return StandardEntry{
        .id = 0,
        .type = EntryType::TRACE_BACKWARDS,
        .timestamp = kTraceRecollectionTimestamp,
        .tid = threadID(),
        .callid = 0,
        .matchid = 0,
        .extra = kTraceId,
    };
  }

  StandardEntry generateRandomEntry() {
    StandardEntry entry{.id = 0,
                        /* First 10 entries as they don't have trace control
                           entries among them. */
                        .type = static_cast<EntryType>(std::rand() % 10),
                        .timestamp = std::rand() % INT64_MAX,
                        .tid = std::rand() % INT32_MAX,
                        .callid = std::rand() % INT32_MAX,
                        .matchid = std::rand() % INT32_MAX,
                        .extra = std::rand() % INT64_MAX};
    return entry;
  }

  void writeRandomEntries(TraceBuffer& buf, int records_count) {
    std::stringstream outstream{};
    PrintEntryVisitor printVisitor(outstream);
    DeltaEncodingVisitor deltaVisitor(printVisitor);
    TimestampTruncatingVisitor visitor(deltaVisitor, 6);

    Logger logger([&buf]() -> TraceBuffer& { return buf; });
    // Write the main service entry before the main content
    auto serviceEntry = generateTraceBackwardsEntry();
    outstream.str("");
    visitor.visit(serviceEntry);
    loggedEntries_.push_back(outstream.str());
    while (records_count > 0) {
      auto entry = generateRandomEntry();
      outstream.str("");
      visitor.visit(entry);
      loggedEntries_.push_back(outstream.str());
      logger.write(std::move(entry));
      --records_count;
    }
  }

  void writeTraceWithRandomEntries(int records_count) {
    writeTraceWithRandomEntries(records_count, records_count);
  }

  void writeTraceWithRandomEntries(
      int records_count,
      int buffer_size,
      bool set_mappings_file = false) {
    auto buffer = manager_.allocateBuffer(buffer_size, dumpPath(), 1, 1);
    ASSERT_NE(buffer, nullptr) << "Unable to allocate the buffer";
    buffer->prefix->header.providers = 0;
    buffer->prefix->header.longContext = kQplId;
    buffer->prefix->header.traceId = kTraceId;
    if (set_mappings_file) {
      auto path = temp_mappings_file_.path().filename().generic_string();
      auto sz = std::min(
          path.size(), sizeof(buffer->prefix->header.memoryMapsFilename) - 1);
      ::memcpy(buffer->prefix->header.memoryMapsFilename, path.c_str(), sz);
      buffer->prefix->header.memoryMapsFilename[sz] = 0;
    }
    writeRandomEntries(buffer->ringBuffer(), records_count);
    int msync_res = msync(buffer->prefix, buffer->totalByteSize, MS_SYNC);
    ASSERT_EQ(msync_res, 0)
        << "Unable to msync the buffer: " << strerror(errno);
  }

  void writeMemoryMappingsFile() {
    // use output stream to write the content
    std::ofstream mappingsFile(temp_mappings_file_.path().generic_string());
    for (const std::string& map : kMappings) {
      mappingsFile << map << std::endl;
    }
    mappingsFile.close();
  }

  fs::path getOnlyTraceFile() {
    auto dir_iter = fs::recursive_directory_iterator(traceFolderPath());
    auto is_file = [](const fs::directory_entry& x) {
      return fs::is_regular_file(x.path());
    };
    auto result = std::find_if(
        fs::recursive_directory_iterator(traceFolderPath()),
        fs::recursive_directory_iterator(),
        is_file);
    // Ensures that at least one trace file was found.
    EXPECT_NE(result, fs::recursive_directory_iterator()) << "No trace found";
    auto path = result->path();
    result++;
    // Ensures that only one trace file was found.
    EXPECT_EQ(result, fs::recursive_directory_iterator())
        << "More than one trace found";
    return path;
  }

  std::string getOnlyTraceFileContents() {
    auto file = getOnlyTraceFile();

    std::stringstream output;
    zstr::ifstream input(file.generic_string());
    output << input.rdbuf();
    return output.str();
  }

  void verifyLogEntriesFromTraceFile() {
    auto traceContents = getOnlyTraceFileContents();
    for (std::string& entry : loggedEntries_) {
      // Compare entry output ignoring first entry id (before "|")
      auto entryOutput = entry.substr(entry.find("|"));
      EXPECT_STRING_CONTAINS(traceContents, entryOutput);
    }
  }

  void verifyMemoryMappingEntries() {
    auto traceContents = getOnlyTraceFileContents();
    for (const std::string& map : kMappings) {
      EXPECT_STRING_CONTAINS(traceContents, map);
    }
  }

  MmapBufferManager manager_;
  std::vector<std::string> loggedEntries_;
  test::TemporaryFile temp_dump_file_;
  test::TemporaryDirectory temp_trace_folder_;
  test::TemporaryFile temp_mappings_file_;
};

TEST_F(MmapBufferTraceWriterTest, testDumpWriteAndRecollectEndToEnd) {
  using ::testing::_;
  writeTraceWithRandomEntries(10);

  std::string testFolder(traceFolderPath());
  std::string testTracePrefix(kTracePrefix);
  auto mockCallbacks = std::make_shared<::testing::NiceMock<MockCallbacks>>();
  MmapBufferTraceWriter traceWriter(
      testFolder, testTracePrefix, 0, mockCallbacks);

  EXPECT_CALL(*mockCallbacks, onTraceStart(kTraceId, 0, _));
  EXPECT_CALL(*mockCallbacks, onTraceEnd(kTraceId));

  traceWriter.writeTrace(dumpPath(), "test", kTraceRecollectionTimestamp);

  verifyLogEntriesFromTraceFile();
}

TEST_F(
    MmapBufferTraceWriterTest,
    testDumpWriteAndRecollectEndToEndWithMappings) {
  using ::testing::_;
  writeTraceWithRandomEntries(10, 10, true);
  writeMemoryMappingsFile();

  std::string testFolder(traceFolderPath());
  std::string testTracePrefix(kTracePrefix);
  auto mockCallbacks = std::make_shared<::testing::NiceMock<MockCallbacks>>();
  MmapBufferTraceWriter traceWriter(
      testFolder, testTracePrefix, 0, mockCallbacks);

  EXPECT_CALL(*mockCallbacks, onTraceStart(kTraceId, 0, _));
  EXPECT_CALL(*mockCallbacks, onTraceEnd(kTraceId));

  traceWriter.writeTrace(dumpPath(), "test", kTraceRecollectionTimestamp);

  verifyLogEntriesFromTraceFile();
  verifyMemoryMappingEntries();
}

TEST_F(
    MmapBufferTraceWriterTest,
    testAbortCallbackIsCalledWhenWriterThrowsException) {
  using ::testing::_;
  writeTraceWithRandomEntries(10);

  // Causes an attempt to access the testFolder by Writer to fail with an
  // exception.
  std::string testFolder("/dev/null");
  std::string testTracePrefix(kTracePrefix);

  auto mockCallbacks = std::make_shared<::testing::NiceMock<MockCallbacks>>();
  MmapBufferTraceWriter traceWriter(
      testFolder, testTracePrefix, 0, mockCallbacks);

  EXPECT_CALL(*mockCallbacks, onTraceAbort(kTraceId, AbortReason::UNKNOWN));

  traceWriter.writeTrace(dumpPath(), "test", kTraceRecollectionTimestamp);
}

TEST_F(
    MmapBufferTraceWriterTest,
    testExceptionIsThrownWhenNothingReadFromBuffer) {
  using ::testing::_;
  writeTraceWithRandomEntries(0, 1);

  std::string testFolder(traceFolderPath());
  std::string testTracePrefix(kTracePrefix);
  auto mockCallbacks = std::make_shared<::testing::NiceMock<MockCallbacks>>();
  MmapBufferTraceWriter traceWriter(
      testFolder, testTracePrefix, 0, mockCallbacks);

  bool caughtException = false;
  try {
    traceWriter.writeTrace(dumpPath(), "test", kTraceRecollectionTimestamp);
  } catch (std::runtime_error& e) {
    ASSERT_STREQ(e.what(), "Unable to read the file-backed buffer.");
    caughtException = true;
  }
  ASSERT_TRUE(caughtException);
}

} // namespace writer
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
