// Copyright 2004-present Facebook. All Rights Reserved.
// @generated SignedSource<<035fe5f5db869eebc3ebf0b7dc02009a>>

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <unistd.h>

#pragma once

namespace facebook {
namespace loom {
namespace entries {

struct __attribute__((packed)) StandardEntry {

  static const uint8_t kSerializationType = 1;

  int32_t id;
  uint8_t type;
  int64_t timestamp;
  int32_t tid;
  int32_t callid;
  int32_t matchid;
  int64_t extra;

  static void pack(const StandardEntry& entry, void* dst, size_t size);
  static void unpack(StandardEntry& entry, const void* src, size_t size);

  static size_t calculateSize(StandardEntry const& entry);
};

struct __attribute__((packed)) FramesEntry {

  static const uint8_t kSerializationType = 2;

  int32_t id;
  uint8_t type;
  int64_t timestamp;
  int32_t tid;
  struct {
    const int64_t* values;
    uint16_t size;
  } frames;

  static void pack(const FramesEntry& entry, void* dst, size_t size);
  static void unpack(FramesEntry& entry, const void* src, size_t size);

  static size_t calculateSize(FramesEntry const& entry);
};

struct __attribute__((packed)) BytesEntry {

  static const uint8_t kSerializationType = 3;

  int32_t id;
  uint8_t type;
  int32_t matchid;
  struct {
    const uint8_t* values;
    uint16_t size;
  } bytes;

  static void pack(const BytesEntry& entry, void* dst, size_t size);
  static void unpack(BytesEntry& entry, const void* src, size_t size);

  static size_t calculateSize(BytesEntry const& entry);
};


uint8_t peek_type(const void* src, size_t len);

} // namespace entries
} // namespace loom
} // namespace facebook
