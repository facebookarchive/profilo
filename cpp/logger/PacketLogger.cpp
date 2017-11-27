// Copyright 2004-present Facebook. All Rights Reserved.

#include "PacketLogger.h"


namespace facebook {
namespace loom {
namespace logger {

PacketLogger::PacketLogger(PacketBufferProvider provider)
  : streamID_(0),
    provider_(provider)
  {}

void PacketLogger::write(void* payload, size_t size) {
  writeAndGetCursor(payload, size);
}

PacketBuffer::Cursor PacketLogger::writeAndGetCursor(
  void* payload,
  size_t size
) {
  if (size == 0) {
    throw std::invalid_argument("size is 0");
  }

  if (payload == nullptr) {
    throw std::invalid_argument("payload is null");
  }

  auto& buffer = provider_();

  PacketBuffer::Cursor cursor = buffer.currentTail();
  bool cursor_set = false;

  StreamID stream_id = streamID_.fetch_add(1, std::memory_order_relaxed);

  size_t offset = 0;
  while (offset < size) {
    const auto kOnePacketSize = sizeof(Packet::data);

    auto remaining = size - offset;
    bool has_next = remaining > kOnePacketSize;
    uint8_t write_size = std::min(kOnePacketSize, remaining);

    Packet packet {
      .stream = stream_id,
      .start = offset == 0,
      .next = has_next,
      .size = write_size,
      .data = {}
    };

    std::memcpy(packet.data, static_cast<char*>(payload) + offset, write_size);

    if (!cursor_set) {
      cursor = buffer.writeAndGetCursor(packet);
      cursor_set = true;
    } else {
      buffer.write(packet);
    }
    offset += write_size;
  }

  return cursor;
}

} } } // facebook::loom::logger
