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

#include <profilo/logger/buffer/Packet.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/logger/lfrb/LockFreeRingBuffer.h>
#include <profilo/mmapbuf/MmapBufferManager.h>
#include <util/common.h>

#include <zlib.h>
#include "../../mmapbuf/MmapBufferManager.h"

namespace test = folly::test;

namespace facebook {
namespace profilo {
namespace mmapbuf {

using Packet = logger::Packet;

constexpr auto kPayloadSize = sizeof(Packet::data);

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

uint32_t
writeRandomEntries(TraceBuffer& buf, int records_count, int buffer_size) {
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
    alignas(4) Packet packet{.data = {}};
    std::memcpy(packet.data, static_cast<char*>(payload), kPayloadSize);
    buf.write(packet);
  }
  return crc;
}

TEST_F(MmapBufferManagerTest, testMmapBufferAllocationCorrectness) {
  const auto kBufferSize = 1000;
  MmapBufferManager bufManager{};

  auto buffer = bufManager.allocateBuffer(kBufferSize, path(), 1, 1);
  ASSERT_NE(buffer, nullptr) << "Unable to allocate the buffer";

  auto crc = writeRandomEntries(buffer->ringBuffer(), kBufferSize, kBufferSize);
  const auto expectedFileSize = sizeof(MmapBufferPrefix) +
      TraceBuffer::calculateAllocationSize(kBufferSize);

  struct stat fileStat;
  fstat(fd(), &fileStat);

  EXPECT_EQ(fileStat.st_size, expectedFileSize);

  int msync_res = msync(buffer->prefix, expectedFileSize, MS_SYNC);
  ASSERT_EQ(msync_res, 0) << "Unable to msync the buffer: " << strerror(errno);

  char* buf = new char[expectedFileSize];
  auto sizeRead = read(fd(), buf, expectedFileSize);

  ASSERT_EQ(sizeRead, expectedFileSize);

  char* ptr = buf + sizeof(MmapBufferPrefix) + sizeof(TraceBuffer);
  auto crc_after = crc32(0L, Z_NULL, 0);
  for (int i = 0; i < kBufferSize; ++i) {
    crc_after = crc32(
        crc_after,
        reinterpret_cast<const unsigned char*>(
            ptr + sizeof(logger::lfrb::TurnSequencer<std::atomic>) +
            offsetof(Packet, data)),
        kPayloadSize);
    ptr += sizeof(logger::lfrb::detail::RingBufferSlot<Packet>);
  }

  EXPECT_EQ(crc, crc_after);
  delete[] buf;
}

TEST_F(MmapBufferManagerTest, testMmapBufferAllocateDeallocate) {
  const auto kBufferSize = 1000;
  const auto expectedFileSize = sizeof(MmapBufferPrefix) +
      TraceBuffer::calculateAllocationSize(kBufferSize);
  void* bufAddress = nullptr;
  {
    MmapBufferManager bufManager{};

    auto buffer = bufManager.allocateBuffer(kBufferSize, path(), 1, 1);
    ASSERT_NE(buffer, nullptr) << "Unable to allocate the buffer";

    struct stat fileStat;
    fstat(fd(), &fileStat);
    EXPECT_EQ(fileStat.st_size, expectedFileSize);

    close(fd());

    bufAddress = buffer->prefix;
    // bufManager destructor removes the buffer
  }

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
