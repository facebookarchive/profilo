/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain a
 * copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include "BufferFileMapHolder.h"

#include <errno.h>
#include <fb/log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace writer {

namespace {
struct FileDescriptor {
  int fd;

  FileDescriptor(const std::string& dump_path) {
    fd = open(dump_path.c_str(), O_RDONLY);
    if (fd == -1) {
      FBLOGE(
          "Unable to open a dump file %s, errno: %s",
          dump_path.c_str(),
          std::strerror(errno));
      throw std::runtime_error("Error while opening a dump file");
    }
  }

  ~FileDescriptor() {
    close(fd);
  }
};
} // namespace

BufferFileMapHolder::BufferFileMapHolder(const std::string& dump_path) {
  FileDescriptor fd(dump_path);
  struct stat fileStat;
  int res = fstat(fd.fd, &fileStat);
  if (res != 0) {
    throw std::runtime_error("Unable to read fstat from the buffer file");
  }
  size = (size_t)fileStat.st_size;
  if (size == 0) {
    throw std::runtime_error("Empty buffer file");
  }
  map_ptr = mmap(nullptr, (size_t)size, PROT_READ, MAP_PRIVATE, fd.fd, 0);
  if (map_ptr == MAP_FAILED) {
    throw std::runtime_error("Failed to map the buffer file");
  }
}

BufferFileMapHolder::~BufferFileMapHolder() {
  munmap(map_ptr, size);
}

} // namespace writer
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
