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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <climits>

#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/logger/lfrb/LockFreeRingBuffer.h>
#include <profilo/mmapbuf/MmapBufferManager.h>
#include <profilo/mmapbuf/MmapBufferManagerTestAccessor.h>
#include <util/common.h>

#include <zlib.h>
#include "../../mmapbuf/MmapBufferManager.h"

namespace test = folly::test;

namespace facebook {
namespace profilo {
namespace mmapbuf {

class MmapBufferManagerTest : public ::testing::Test {
 protected:
  MmapBufferManagerTest() : ::testing::Test(), temp_dump_file_("test_dump") {
    std::srand(std::time(nullptr));
  }

  int fd() {
    return temp_dump_file_.fd();
  }

  const std::string& path() {
    return temp_dump_file_.path().generic_string();
  }

  test::TemporaryFile temp_dump_file_;
};

const int kPayloadSize = sizeof(logger::Packet);

struct __attribute__((packed)) TestPacket {
  char payload[kPayloadSize];
};

namespace lfrb = logger::lfrb;

using TestBuffer = lfrb::LockFreeRingBuffer<TestPacket>;
using TestBufferSlot = lfrb::detail::RingBufferSlot<TestPacket>;
using TestBufferHolder = lfrb::LockFreeRingBufferHolder<TestPacket>;

uint32_t
writeRandomEntries(TestBuffer& buf, int records_count, int buffer_size) {
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
    for (int j = 0; j < kPayloadSize; ++j) {
      payload[j] = std::rand() % CHAR_MAX;
    }
    crc = crc32(
        crc, reinterpret_cast<const unsigned char*>(payload), kPayloadSize);
    alignas(4) TestPacket packet{.payload = {}};
    std::memcpy(packet.payload, static_cast<char*>(payload), kPayloadSize);
    buf.write(packet);
  }
  return crc;
}

uint32_t calculateBufferSizeBytes(uint32_t buffer_size) {
  const auto testBufferSize =
      sizeof(TestBuffer) + buffer_size * sizeof(TestBufferSlot);
  return sizeof(MmapBufferPrefix) + testBufferSize;
}

TEST_F(MmapBufferManagerTest, testMmapBufferAllocationCorrectness) {
  const auto kBufferSize = 1000;
  MmapBufferManager bufManager{};
  MmapBufferManagerTestAccessor bufManagerAccessor(bufManager);

  bool res = bufManager.allocateBuffer(kBufferSize, path(), 1, 1);

  ASSERT_EQ(res, true) << "Unable to allocate the buffer";

  TestBufferHolder ringBuffer = TestBuffer::allocateAt(
      kBufferSize, bufManagerAccessor.mmapBufferPointer());
  auto crc = writeRandomEntries(*ringBuffer, kBufferSize, kBufferSize);

  const auto expectedFileSize = calculateBufferSizeBytes(kBufferSize);

  struct stat fileStat;
  fstat(fd(), &fileStat);

  EXPECT_EQ(fileStat.st_size, expectedFileSize);

  int msync_res =
      msync(bufManagerAccessor.mmapPointer(), expectedFileSize, MS_SYNC);
  ASSERT_EQ(msync_res, 0) << "Unable to msync the buffer: " << strerror(errno);

  char* buf = new char[expectedFileSize];
  auto sizeRead = read(fd(), buf, expectedFileSize);

  ASSERT_EQ(sizeRead, expectedFileSize);

  char* ptr = buf + sizeof(MmapBufferPrefix) + sizeof(TestBuffer);
  auto crc_after = crc32(0L, Z_NULL, 0);
  for (int i = 0; i < kBufferSize; ++i) {
    crc_after = crc32(
        crc_after,
        reinterpret_cast<const unsigned char*>(
            ptr + sizeof(lfrb::TurnSequencer<std::atomic>)),
        sizeof(TestPacket));
    ptr += sizeof(TestBufferSlot);
  }

  EXPECT_EQ(crc, crc_after);
  delete[] buf;
  bufManager.deallocateBuffer();
}

TEST_F(MmapBufferManagerTest, testMmapBufferAllocateDeallocate) {
  const auto kBufferSize = 1000;
  MmapBufferManager bufManager{};
  MmapBufferManagerTestAccessor bufManagerAccessor(bufManager);

  bool res = bufManager.allocateBuffer(kBufferSize, path(), 1, 1);

  ASSERT_EQ(res, true) << "Unable to allocate the buffer";

  const auto expectedFileSize = calculateBufferSizeBytes(kBufferSize);
  struct stat fileStat;
  fstat(fd(), &fileStat);
  EXPECT_EQ(fileStat.st_size, expectedFileSize);

  close(fd());

  auto bufAddress = bufManagerAccessor.mmapPointer();
  bufManager.deallocateBuffer();

  void* res_mmap = mmap(
      bufAddress,
      expectedFileSize,
      MAP_FIXED,
      PROT_READ | MAP_ANONYMOUS,
      -1,
      0);
  // Already unmapped, expect mapping to succeed
  EXPECT_EQ(bufAddress, res_mmap);
  int res_open = open(path().c_str(), 0, O_RDONLY);
  EXPECT_EQ(-1, res_open);
  EXPECT_EQ(ENOENT, errno);
  munmap(res_mmap, expectedFileSize);
}

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
