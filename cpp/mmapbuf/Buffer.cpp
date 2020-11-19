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

#include "Buffer.h"
#include <profilo/logger/buffer/RingBuffer.h>

#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <memory>

#include <errno.h>

#include <fb/log.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {

namespace {

static size_t calculateBufferSize(size_t entryCount) {
  return sizeof(MmapBufferPrefix) + sizeof(TraceBuffer) +
      entryCount * sizeof(TraceBufferSlot);
}

} // namespace

Buffer::Buffer(std::string const& path, size_t entryCount) {
  int fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    throw std::system_error(
        errno, std::system_category(), "Cannot open file " + path);
  }

  size_t totalSize = calculateBufferSize(entryCount);

  // In order to allocate file size of N bytes we seek to (N-1)th position and
  // just write single byte at the end. This allows us to avoid filling the
  // whole file before mmap() call.
  auto res = lseek(fd, totalSize - 1, SEEK_SET);
  if (res == -1) {
    close(fd);
    throw std::system_error(
        errno, std::system_category(), "Cannot lseek file " + path);
  }

  char byte = 0x00;
  res = write(fd, &byte, 1);
  if (res == -1) {
    close(fd);
    throw std::system_error(
        errno, std::system_category(), "Cannot write a byte " + path);
  }

  void* map_ptr =
      mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map_ptr == MAP_FAILED) {
    close(fd);
    throw std::system_error(
        errno, std::system_category(), "Cannot mmap file " + path);
  }

  close(fd);

  // Initialize MmapBuffer in the created mmap area.
  auto map_chr = reinterpret_cast<char*>(map_ptr);

  // Initialize a prefix at the beginning of the space,
  // and set buffer to immediately after the prefix.
  prefix = new (map_chr) MmapBufferPrefix();
  buffer = map_chr + sizeof(MmapBufferPrefix);
  this->path = path;
  this->entryCount = entryCount;
  this->totalByteSize = totalSize;
  this->file_backed_ = true;
}

Buffer::Buffer(size_t entryCount) {
  size_t totalSize = calculateBufferSize(entryCount);

  auto mem = new char[totalSize];
  prefix = new (mem) MmapBufferPrefix();
  buffer = mem + sizeof(MmapBufferPrefix);
  this->totalByteSize = totalSize;
  this->entryCount = entryCount;
  this->file_backed_ = false;
}

Buffer::Buffer(Buffer&& other)
    : path(std::move(other.path)),
      entryCount(other.entryCount),
      totalByteSize(other.totalByteSize),
      prefix(other.prefix),
      buffer(other.buffer),
      file_backed_(other.file_backed_) {
  other.entryCount = 0;
  other.totalByteSize = 0;
  other.prefix = nullptr;
  other.buffer = nullptr;
  other.file_backed_ = false;
}

Buffer& Buffer::operator=(Buffer&& other) {
  this->~Buffer();

  path = std::move(other.path);
  entryCount = other.entryCount;
  totalByteSize = other.totalByteSize;
  prefix = other.prefix;
  buffer = other.buffer;
  file_backed_ = other.file_backed_;

  other.buffer = other.prefix = nullptr;
  other.entryCount = other.totalByteSize = 0;
  other.file_backed_ = false;
  return *this;
}

Buffer::~Buffer() {
  if (buffer == nullptr) {
    return;
  }

  prefix->~MmapBufferPrefix();
  if (file_backed_) {
    // mmap mode: remove the mapping and the file
    munmap(prefix, totalByteSize);
    unlink(path.c_str());
  } else {
    // anonymous mode: delete the backing memory
    delete[] reinterpret_cast<char*>(prefix);
  }
}

void Buffer::rename(std::string const& new_path) {
  if (::rename(path.c_str(), new_path.c_str())) {
    throw std::system_error(
        errno,
        std::system_category(),
        "Failed to rename mmap buffer file " + path + " to " + new_path);
  }
  path = new_path;
}

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
