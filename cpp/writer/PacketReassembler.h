// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <profilo/Packet.h>

#include <functional>
#include <list>
#include <vector>

namespace facebook {
namespace profilo {

using namespace logger;

namespace writer {
namespace detail {

struct PacketStream {
  StreamID stream;
  std::vector<char> data;

  PacketStream() = default;
  PacketStream(PacketStream&& other) = default;
  PacketStream(PacketStream const& other) = delete;
  PacketStream& operator=(PacketStream const& other) = delete;
  PacketStream& operator=(PacketStream&& other) = default;
};

} // detail

class PacketReassembler {
public:
  using PayloadCallback = std::function<void(const void*, size_t)>;

  PacketReassembler(PayloadCallback callback);
  void process(Packet const& packet);
  void processBackwards(Packet const& packet);

private:
  static constexpr auto kStreamPoolSize = 8;

  std::list<detail::PacketStream> active_streams_;
  std::list<detail::PacketStream> pooled_streams_;
  PayloadCallback callback_;

  detail::PacketStream newStream();
  void startNewStream(Packet& packet);
  void recycleStream(detail::PacketStream stream);
};

} } } // facebook::profilo::writer
