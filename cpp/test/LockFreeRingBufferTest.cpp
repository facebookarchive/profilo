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
#include <memory>

#include <profilo/logger/lfrb/LockFreeRingBuffer.h>
#include <profilo/util/common.h>

#include <zlib.h>

namespace test = folly::test;

namespace facebook {
namespace profilo {
namespace logger {
namespace lfrb {

const int kPayloadSize = 64;

struct __attribute__((packed)) TestPacket {
  char payload[kPayloadSize];
};

using TestBuffer = LockFreeRingBuffer<TestPacket>;
using TestBufferSlot = detail::RingBufferSlot<TestPacket>;

//
// This test accessor primarily exists to avoid bringing in the Buffer
// LFRB holder into these tests.
//
class LockFreeRingBufferTestAccessor {
 public:
  static TestBuffer* allocate(size_t count) {
    char* mem = new char[TestBuffer::calculateAllocationSize(count)];
    return allocateAt(count, mem);
  }
  static TestBuffer* allocateAt(size_t count, void* ptr) {
    return TestBuffer::allocateAt(count, ptr);
  }

  static void destroy(TestBuffer* buf) {
    buf->~LockFreeRingBuffer();
    delete[](char*) buf;
  }
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

TEST(LockFreeRingBufferTest, testAllocationCorrectness) {
  constexpr auto kBufferSize = 10;
  constexpr auto kBufferStructSize = sizeof(TestBuffer);
  constexpr auto kBufferSlotStructSize = sizeof(TestBufferSlot);
  constexpr auto bufLen = TestBuffer::calculateAllocationSize(kBufferSize);
  char buf[bufLen] = {0};
  TestBuffer* ringBuffer =
      LockFreeRingBufferTestAccessor::allocateAt(kBufferSize, buf);

  auto crc = writeRandomEntries(*ringBuffer, kBufferSize, kBufferSize);

  auto crc_after = crc32(0L, Z_NULL, 0);
  for (char* ptr = buf + kBufferStructSize; ptr < buf + bufLen;
       ptr += kBufferSlotStructSize) {
    crc_after = crc32(
        crc_after,
        reinterpret_cast<const unsigned char*>(
            ptr + sizeof(TurnSequencer<std::atomic>)),
        sizeof(TestPacket));
  }

  EXPECT_EQ(crc, crc_after);
}

} // namespace lfrb
} // namespace logger
} // namespace profilo
} // namespace facebook
