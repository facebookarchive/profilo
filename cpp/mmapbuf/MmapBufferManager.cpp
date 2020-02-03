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

#include "MmapBufferManager.h"

#include <profilo/logger/buffer/RingBuffer.h>

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

MmapBufferManager::MmapBufferManager()
    : path_(""), size_(0), buffer_prefix_(nullptr) {}

bool MmapBufferManager::allocateBuffer(
    int32_t buffer_size,
    const std::string& path,
    int32_t version_code,
    int64_t config_id) {
  int fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    FBLOGE("Cannot open the file \"%s\": %s", path.c_str(), strerror(errno));
    return false;
  }

  size_t totalSizeBytes = sizeof(MmapBufferPrefix) + sizeof(TraceBuffer) +
      buffer_size * sizeof(TraceBufferSlot);

  // In order to allocate file size of N bytes we seek to (N-1)th position and
  // just write single byte at the end. This allows us to avoid filling the
  // whole file before mmap() call.
  auto res = lseek(fd, totalSizeBytes - 1, SEEK_SET);
  if (res == -1) {
    FBLOGE("Lseek failed: %s", strerror(errno));
    close(fd);
    return false;
  }

  char byte = 0x00;
  res = write(fd, &byte, 1);
  if (res == -1) {
    FBLOGE("Single byte write failed: %s", strerror(errno));
    close(fd);
    return false;
  }

  void* map_ptr =
      mmap(nullptr, totalSizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map_ptr == MAP_FAILED) {
    FBLOGE("Mmap failed: %s", strerror(errno));
    close(fd);
    return false;
  }

  close(fd);

  size_ = totalSizeBytes;
  path_ = path;
  // Initialize MmapBuffer in the created mmap area.
  MmapBufferPrefix* mmapBufferPrefix =
      new (reinterpret_cast<char*>(map_ptr)) MmapBufferPrefix();
  buffer_prefix_.store(mmapBufferPrefix);

  mmapBufferPrefix->header.bufferVersion = RingBuffer::kVersion;
  mmapBufferPrefix->header.size = (uint32_t)buffer_size;
  mmapBufferPrefix->header.versionCode = version_code;
  mmapBufferPrefix->header.configId = config_id;

  // Buffer initialization
  RingBuffer::init(
      reinterpret_cast<char*>(map_ptr) + sizeof(MmapBufferPrefix), buffer_size);

  return true;
}

void MmapBufferManager::deallocateBuffer() {
  RingBuffer::destroy();
  munmap(buffer_prefix_.load(), size_);
  unlink(path_.c_str());
}

void MmapBufferManager::updateHeader(
    int32_t providers,
    int32_t long_context,
    int64_t normal_trace_id,
    int64_t memory_trace_id) {
  MmapBufferPrefix* bufferPrefix = buffer_prefix_.load();
  if (bufferPrefix == nullptr) {
    return;
  }
  bufferPrefix->header.providers = providers;
  bufferPrefix->header.longContext = long_context;
  bufferPrefix->header.normalTraceId = normal_trace_id;
  bufferPrefix->header.inMemoryTraceId = memory_trace_id;
}

void MmapBufferManager::updateSessionId(const std::string& session_id) {
  MmapBufferPrefix* bufferPrefix = buffer_prefix_.load();
  if (bufferPrefix == nullptr) {
    return;
  }
  auto sz = std::min(
      session_id.size(), (size_t)MmapBufferHeader::kSessionIdLength - 1);
  session_id.copy(bufferPrefix->header.sessionId, sz);
  bufferPrefix->header.sessionId[sz] = 0;
}

fbjni::local_ref<MmapBufferManager::jhybriddata> MmapBufferManager::initHybrid(
    fbjni::alias_ref<jclass>) {
  return makeCxxInstance();
}

void MmapBufferManager::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", MmapBufferManager::initHybrid),
      makeNativeMethod(
          "nativeAllocateBuffer", MmapBufferManager::allocateBuffer),
      makeNativeMethod(
          "nativeDeallocateBuffer", MmapBufferManager::deallocateBuffer),
      makeNativeMethod("nativeUpdateHeader", MmapBufferManager::updateHeader),
      makeNativeMethod(
          "nativeUpdateSessionId", MmapBufferManager::updateSessionId),
  });
}

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
