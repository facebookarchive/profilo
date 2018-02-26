// Copyright 2004-present Facebook. All Rights Reserved.

#include <vector>

#include <loom/logger/lfrb/LockFreeRingBuffer.h>
#include <loom/PacketLogger.h>
#include <loom/writer/PacketReassembler.h>

#include <gtest/gtest.h>

namespace facebook {
namespace profilo {

using namespace logger;
using namespace writer;

const size_t kItemSize = sizeof(uint16_t);
const size_t kItems = 512;

TEST(Logger, testPacketizedWrite) {
  std::vector<uint16_t> data(kItems);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i;
  }

  PacketBuffer buffer(1000);
  PacketLogger logger([&]() -> PacketBuffer& {
    return buffer;
  });

  //
  // Try different sized writes from 1 to data.size().
  // Assert that the data in each case is correct and that the
  // PacketReassembler sees exactly one payload.
  //
  for (size_t i = 1; i <= data.size(); ++i) {
    PacketBuffer::Cursor cursor = buffer.currentHead();
    logger.write(data.data(), i * kItemSize);

    size_t calls = 0;

    PacketReassembler reassembler([&](const void* read_data, size_t size) {
      EXPECT_EQ(size, i * kItemSize) << "read must be the same size as write";
      const uint16_t* idata = reinterpret_cast<const uint16_t*>(read_data);

      for (size_t j = 0; j < size / kItemSize; ++j) {
        EXPECT_EQ(j, idata[j]) << "data must be the same";
      }

      ++calls;
    });

    Packet packet;
    while (buffer.tryRead(packet, cursor)) {
      reassembler.process(packet);
      cursor.moveForward();
    }

    EXPECT_EQ(calls, 1) << "must read exactly one payload";
  }
}

TEST(Logger, testPacketizedWriteBackwards) {
  std::vector<uint16_t> data(kItems);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i;
  }
  //
  // Try different sized writes from 1 to data.size().
  // Assert that the data in each case is correct and that the
  // PacketReassembler sees exactly one payload.
  //
  for (size_t i = 1; i <= data.size(); ++i) {
    PacketBuffer buffer(1000);
    PacketLogger logger([&]() -> PacketBuffer& {
      return buffer;
    });

    logger.write(data.data(), i * kItemSize);

    size_t calls = 0;

    PacketReassembler reassembler([&](const void* read_data, size_t size) {
      EXPECT_EQ(size, i * kItemSize) << "read must be the same size as write";
      const uint16_t* idata = reinterpret_cast<const uint16_t*>(read_data);

      for (size_t j = 0; j < size / kItemSize; ++j) {
        EXPECT_EQ(j, idata[j]) << "data must be the same";
      }
      ++calls;
    });

    auto cursor = buffer.currentHead();
    cursor.moveBackward();

    Packet packet;
    while (buffer.tryRead(packet, cursor)) {
      reassembler.processBackwards(packet);
      if (!cursor.moveBackward()) {
        break;
      }
    }
    EXPECT_EQ(calls, 1) << "must read exactly one payload";
  }
}

} } // facebook::profilo
