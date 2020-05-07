// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/experimental/TestUtil.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <zlib.h>
#include <zstr/zstr.hpp>
#include <climits>
#include <ostream>
#include <sstream>
#include <vector>

#include <profilo/entries/Entry.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/MmapBufferManager.h>
#include <profilo/mmapbuf/MmapBufferPrefix.h>
#include <profilo/mmapbuf/writer/MmapBufferTraceWriter.h>
#include <profilo/writer/DeltaEncodingVisitor.h>
#include <profilo/writer/PrintEntryVisitor.h>
#include <profilo/writer/TimestampTruncatingVisitor.h>
#include <util/common.h>

using namespace facebook::profilo::logger;
using namespace facebook::profilo::entries;

namespace test = folly::test;
namespace fs = boost::filesystem;

namespace facebook {
namespace profilo {
namespace mmapbuf {

class MmapBufferManagerTestAccessor {
 public:
  explicit MmapBufferManagerTestAccessor(MmapBufferManager& bufferManager)
      : bufferManager_(bufferManager) {}

  void* mmapBufferPointer() {
    return reinterpret_cast<void*>(
        reinterpret_cast<char*>(bufferManager_.buffer_prefix_.load()) +
        sizeof(MmapBufferPrefix));
  }

  void* mmapPointer() {
    return reinterpret_cast<void*>(bufferManager_.buffer_prefix_.load());
  }

  uint32_t size() {
    return bufferManager_.size_;
  }

 private:
  MmapBufferManager& bufferManager_;
};

namespace writer {

constexpr char kTraceFolder[] = "mmabbuf-test-trace-folder";
constexpr char kTracePrefix[] = "mmabbuf-test-trace-";
constexpr int64_t kTraceId = 222;
constexpr int32_t kQplId = 33444;
constexpr int64_t kTraceRecollectionTimestamp = 1234567;
constexpr int32_t kConfigId = 11;
constexpr int32_t kBuildId = 17;

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

struct StandardEntryWrapper {
  StandardEntryWrapper(StandardEntry& standard_entry)
      : entry(standard_entry), output(precomputeTraceOutput(entry)) {}

  static std::string precomputeTraceOutput(
      const StandardEntry& standard_entry) {
    outstream().str("");
    visitor().visit(standard_entry);
    return outstream().str();
  }

  static std::stringstream& outstream() {
    static std::stringstream ss{};
    return ss;
  }

  static EntryVisitor& visitor() {
    // Replicating part of visitor chain to get the expected output
    static PrintEntryVisitor printVisitor(outstream());
    static DeltaEncodingVisitor deltaVisitor(printVisitor);
    static TimestampTruncatingVisitor timestampVisitor(deltaVisitor, 6);
    return timestampVisitor;
  }

  StandardEntry entry;
  std::string output;
};

class MmapBufferTraceWriterTest : public ::testing::Test {
 protected:
  MmapBufferTraceWriterTest()
      : ::testing::Test(),
        loggedEntries_(),
        temp_dump_file_("test_dump"),
        temp_trace_folder_(kTraceFolder) {
    std::srand(std::time(nullptr));
  }

  int dumpFd() {
    return temp_dump_file_.fd();
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
    Logger logger([&buf]() -> PacketBuffer& { return buf; });
    // Write the main service entry before the main content
    auto serviceEntry = generateTraceBackwardsEntry();
    loggedEntries_.push_back(StandardEntryWrapper(serviceEntry));
    while (records_count > 0) {
      auto entry = generateRandomEntry();
      loggedEntries_.push_back(StandardEntryWrapper(entry));
      logger.write(std::move(entry));
      --records_count;
    }
  }

  void writeTraceWithRandomEntries(int records_count) {
    writeTraceWithRandomEntries(records_count, records_count);
  }

  void writeTraceWithRandomEntries(int records_count, int buffer_size) {
    MmapBufferManager bufManager{};
    MmapBufferManagerTestAccessor bufManagerAccessor(bufManager);
    bool res = bufManager.allocateBuffer(buffer_size, dumpPath(), 1, 1);
    bufManager.updateHeader(0, 0, 0, kTraceId);
    bufManager.updateId("272c3f80-f076-5a89-e265-60dcf407373b");
    ASSERT_EQ(res, true) << "Unable to allocate the buffer";
    TraceBufferHolder ringBuffer = TraceBuffer::allocateAt(
        buffer_size, bufManagerAccessor.mmapBufferPointer());
    writeRandomEntries(*ringBuffer, records_count);
    int msync_res = msync(
        bufManagerAccessor.mmapPointer(), bufManagerAccessor.size(), MS_SYNC);
    ASSERT_EQ(msync_res, 0)
        << "Unable to msync the buffer: " << strerror(errno);
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
    EXPECT_NE(result, fs::recursive_directory_iterator());
    auto path = result->path();
    result++;
    // Ensures that only one trace file was found.
    EXPECT_EQ(result, fs::recursive_directory_iterator());
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
    for (auto entry : loggedEntries_) {
      // Compare entry output ignoring first entry id (before "|")
      auto entryOutput = entry.output.substr(entry.output.find("|"));
      EXPECT_STRING_CONTAINS(traceContents, entryOutput);
    }
  }

  std::vector<StandardEntryWrapper> loggedEntries_;
  test::TemporaryFile temp_dump_file_;
  test::TemporaryDirectory temp_trace_folder_;
};

TEST_F(MmapBufferTraceWriterTest, testDumpWriteAndRecollectEndToEnd) {
  using ::testing::_;
  writeTraceWithRandomEntries(10);

  std::string testFolder(traceFolderPath());
  std::string testTracePrefix(kTracePrefix);
  auto mockCallbacks = std::make_shared<::testing::NiceMock<MockCallbacks>>();
  MmapBufferTraceWriter traceWriter(testFolder, testTracePrefix, mockCallbacks);

  EXPECT_CALL(*mockCallbacks, onTraceStart(kTraceId, 0, _));
  EXPECT_CALL(*mockCallbacks, onTraceEnd(kTraceId));

  traceWriter.writeTrace(
      dumpPath(), kQplId, "test", kTraceRecollectionTimestamp);

  verifyLogEntriesFromTraceFile();
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
  MmapBufferTraceWriter traceWriter(testFolder, testTracePrefix, mockCallbacks);

  EXPECT_CALL(*mockCallbacks, onTraceAbort(kTraceId, AbortReason::UNKNOWN));

  traceWriter.writeTrace(
      dumpPath(), kQplId, "test", kTraceRecollectionTimestamp);
}

TEST_F(
    MmapBufferTraceWriterTest,
    testExceptionIsThrownWhenNothingReadFromBuffer) {
  using ::testing::_;
  writeTraceWithRandomEntries(0, 1);

  std::string testFolder(traceFolderPath());
  std::string testTracePrefix(kTracePrefix);
  auto mockCallbacks = std::make_shared<::testing::NiceMock<MockCallbacks>>();
  MmapBufferTraceWriter traceWriter(testFolder, testTracePrefix, mockCallbacks);

  bool caughtException = false;
  try {
    traceWriter.writeTrace(
        dumpPath(), kQplId, "test", kTraceRecollectionTimestamp);
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
