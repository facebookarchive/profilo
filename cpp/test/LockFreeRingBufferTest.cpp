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

#include <folly/experimental/TestUtil.h>
#include <gtest/gtest.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <climits>

#include <profilo/logger/lfrb/LockFreeRingBuffer.h>
#include <util/common.h>

#include <zlib.h>

namespace test = folly::test;

namespace facebook {
namespace profilo {
namespace logger {
namespace lfrb {

class LockFreeRingBufferTest : public ::testing::Test {
 protected:
  LockFreeRingBufferTest() : ::testing::Test(), temp_dump_file_("test_dump") {}

  int fd() {
    return temp_dump_file_.fd();
  }

  test::TemporaryFile temp_dump_file_;
};

const int kPayloadSize = 64;

struct __attribute__((packed)) TestPacket {
  char payload[kPayloadSize];
};

uint32_t writeRandomEntries(
    LockFreeRingBuffer<TestPacket>& buf,
    int records_count,
    int buffer_size) {
  // CRC is computed for the last buffer_size packets.
  auto start_crc_index = records_count - buffer_size;
  if (start_crc_index < 0) {
    start_crc_index = 0;
  }
  auto crc = 0;
  for (int i = 0; i < records_count; ++i) {
    if (i == start_crc_index) {
      crc = crc32(0, Z_NULL, 0);
    }
    char payload[kPayloadSize];
    for (int i = 0; i < kPayloadSize; i++) {
      payload[i] = std::rand() % CHAR_MAX;
    }
    crc = crc32(
        crc, reinterpret_cast<const unsigned char*>(payload), kPayloadSize);
    alignas(4) TestPacket packet{.payload = {}};
    std::memcpy(packet.payload, static_cast<char*>(payload), kPayloadSize);
    buf.write(packet);
  }
  return crc;
}

uint32_t readDumpCrc32(int fd, int records_count) {
  TestPacket* bufferDump = reinterpret_cast<TestPacket*>(mmap(
      nullptr,
      sizeof(TestPacket) * records_count,
      PROT_READ,
      MAP_PRIVATE,
      fd,
      0));
  auto crc = crc32(0, Z_NULL, 0);
  for (int i = 0; i < records_count; ++i) {
    crc = crc32(
        crc,
        reinterpret_cast<const unsigned char*>(bufferDump[i].payload),
        kPayloadSize);
  }
  munmap(bufferDump, kPayloadSize);
  return crc;
}

TEST_F(LockFreeRingBufferTest, testEmptyBufDump) {
  const auto kBufferSize = 10;
  LockFreeRingBuffer<TestPacket> buf(kBufferSize);
  int dumpFD = fd();
  buf.dumpDataToFile(dumpFD);
  struct stat dumpStat;
  fstat(dumpFD, &dumpStat);

  EXPECT_EQ(dumpStat.st_size, 0);
}

TEST_F(LockFreeRingBufferTest, testDumpCorrectness) {
  const auto kBufferSize = 7;
  LockFreeRingBuffer<TestPacket> buf(kBufferSize);
  auto crc = writeRandomEntries(buf, kBufferSize, kBufferSize);
  int dumpFD = fd();
  buf.dumpDataToFile(dumpFD);
  auto crc_after = readDumpCrc32(dumpFD, kBufferSize);

  EXPECT_EQ(crc, crc_after);
}

TEST_F(LockFreeRingBufferTest, testSmallBufDump) {
  const auto kBufferSize = 10;
  LockFreeRingBuffer<TestPacket> buf(kBufferSize);
  const auto kRecords = 5;
  auto crc = writeRandomEntries(buf, kRecords, kBufferSize);
  int dumpFD = fd();
  buf.dumpDataToFile(dumpFD);
  auto crc_after = readDumpCrc32(dumpFD, kRecords);

  EXPECT_EQ(crc, crc_after);
}

TEST_F(LockFreeRingBufferTest, testBufDumpAfterOverflow) {
  const auto kBufferSize = 10;
  LockFreeRingBuffer<TestPacket> buf(kBufferSize);
  const auto kRecords = 25;
  auto crc = writeRandomEntries(buf, kRecords, kBufferSize);
  int dumpFD = fd();
  buf.dumpDataToFile(dumpFD);
  auto crc_after = readDumpCrc32(dumpFD, kBufferSize);

  EXPECT_EQ(crc, crc_after);
}

} // namespace lfrb
} // namespace logger
} // namespace profilo
} // namespace facebook
