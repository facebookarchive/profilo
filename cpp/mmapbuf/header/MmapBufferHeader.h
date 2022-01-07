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

#pragma once

#include <stdio.h>
#include <unistd.h>
#include <stdexcept>

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace header {

constexpr static uint64_t kMagic = 0x306c3166307270; // pr0f1l0
constexpr static uint64_t kVersion = 8;

//
// Static header for primary buffer verification.
// It's structure is fixed and should not be changed.
// Must be 8-byte aligned.
//
struct __attribute__((packed)) MmapStaticHeader {
  // Fixed value. Do not change.
  const uint64_t magic = kMagic;
  // Version denotes the file format structure and should be incremented on
  // format change or TraceBuffer changes.
  const uint64_t version = kVersion;
};

//
// Contains Profilo service records for correct trace reconstruction from
// file. If format is modified, kVersion must be incremented.
//
struct __attribute__((packed)) alignas(8) MmapBufferHeader {
  constexpr static auto kSessionIdLength = 40;
  constexpr static auto kMemoryMapsFilePathLength = 512;
  uint16_t bufferVersion;
  int64_t configId;
  int32_t versionCode;
  uint32_t size;
  // Currently turned on set of providers.
  int32_t providers;
  int64_t longContext;
  int64_t traceId;
  pid_t pid;
  char sessionId[kSessionIdLength];
  char memoryMapsFilePath[kMemoryMapsFilePathLength];
};

//
// We are using flexible size array for proper allocation in the memory mapped
// space.
//
// The mmap buffer file has the following format.
// [ Static header (16 bytes) Magic + Version ] - Fixed at build time.
// [ Buffer Header (8-byte aligned)           ] - Dynamic state of Ring Buffer
struct __attribute__((packed)) alignas(8) MmapBufferPrefix {
  MmapStaticHeader staticHeader;
  MmapBufferHeader header;
};

//
// We go through the template indirection in order to have meaningful failure
// messages that show the actual size.
//
template <typename ToCheck, std::size_t RealSize = sizeof(MmapBufferHeader)>
struct check_size_hdr {
  static_assert(RealSize % 8 == 0, "Size must be 8-byte aligned");
};
struct check_size_hdr_ {
  check_size_hdr<MmapBufferHeader> check;
};

template <typename ToCheck, std::size_t RealSize = sizeof(MmapBufferPrefix)>
struct check_size_buf_prefix {
  static_assert(RealSize % 8 == 0, "Size must be 8-byte aligned");
};
struct check_size_buf_prefix_ {
  check_size_hdr<MmapBufferPrefix> check;
};

} // namespace header
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
