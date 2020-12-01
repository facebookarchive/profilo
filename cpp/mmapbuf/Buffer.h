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

#include <string>
#include <type_traits>

#include <profilo/Logger.h>
#include <profilo/logger/buffer/TraceBuffer.h>
#include <profilo/mmapbuf/header/MmapBufferHeader.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {

using MmapBufferPrefix = header::MmapBufferPrefix;

///
/// A holder class for a chunk of (optionally file-backed) memory
/// to place a TraceBuffer in.
///
/// In case it's file-backed, it keeps track of the file path
/// and the file header.
///
struct Buffer {
  // Construct a Buffer from an mmapped file.
  Buffer(std::string const& path, size_t entryCount);
  // Construct a Buffer from anonymous memory.
  explicit Buffer(size_t entryCount);

  Buffer(Buffer const&) = delete;
  Buffer(Buffer&&);

  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&&);
  ~Buffer();

  void rename(std::string const& path);

  TraceBuffer& ringBuffer() {
    return *lfrb_;
  }

  Logger& logger() {
    return logger_;
  }

  std::string path = "";
  size_t entryCount = 0;
  size_t totalByteSize = 0;
  MmapBufferPrefix* prefix = nullptr;
  void* buffer = nullptr;

 private:
  bool file_backed_ = false;
  TraceBuffer* lfrb_ = nullptr;
  Logger logger_{{[this]() -> TraceBuffer& { return this->ringBuffer(); }},
                 Logger::kDefaultInitialID};
};

namespace {
struct BufferTests {
  static_assert(!std::is_copy_constructible<Buffer>::value, "");
  static_assert(!std::is_copy_assignable<Buffer>::value, "");
  static_assert(std::is_move_constructible<Buffer>::value, "");
  static_assert(std::is_move_assignable<Buffer>::value, "");
};
} // namespace

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
