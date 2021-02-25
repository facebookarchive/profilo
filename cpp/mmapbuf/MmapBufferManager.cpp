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
#include <errno.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <system_error>

#include <fb/log.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/header/MmapBufferHeader.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {

fbjni::local_ref<JBuffer::javaobject>
MmapBufferManager::allocateBufferAnonymousForJava(int32_t buffer_size) {
  return JBuffer::makeJBuffer(allocateBufferAnonymous(buffer_size));
}

std::shared_ptr<Buffer> MmapBufferManager::allocateBufferAnonymous(
    int32_t buffer_size) {
  std::shared_ptr<Buffer> buffer = nullptr;
  try {
    buffer = std::make_shared<Buffer>((size_t)buffer_size);
  } catch (std::exception& ex) {
    FBLOGE("%s", ex.what());
    return nullptr;
  }

  {
    WriterLock lock(&buffers_lock_);
    buffers_.push_back(buffer);
  }

  return buffer;
}

fbjni::local_ref<JBuffer::javaobject>
MmapBufferManager::allocateBufferFileForJava(
    int32_t buffer_size,
    const std::string& path,
    int32_t version_code,
    int64_t config_id) {
  auto buffer = allocateBufferFile(buffer_size, path, version_code, config_id);
  if (buffer == nullptr) {
    throw std::invalid_argument("Could not allocate file-backed buffer");
  }
  return JBuffer::makeJBuffer(buffer);
}

std::shared_ptr<Buffer> MmapBufferManager::allocateBufferFile(
    int32_t buffer_size,
    const std::string& path,
    int32_t version_code,
    int64_t config_id) {
  std::shared_ptr<Buffer> buffer = nullptr;
  try {
    buffer = std::make_shared<Buffer>(path, (size_t)buffer_size);
  } catch (std::system_error& ex) {
    FBLOGE("%s", ex.what());
    return nullptr;
  }

  buffer->prefix->header.bufferVersion = RingBuffer::kVersion;
  buffer->prefix->header.size = (size_t)buffer_size;
  buffer->prefix->header.versionCode = version_code;
  buffer->prefix->header.configId = config_id;

  {
    WriterLock lock(&buffers_lock_);
    buffers_.push_back(buffer);
  }

  // Pass the buffer to the global singleton
  return buffer;
}

fbjni::local_ref<MmapBufferManager::jhybriddata> MmapBufferManager::initHybrid(
    fbjni::alias_ref<jclass>) {
  return makeCxxInstance();
}

bool MmapBufferManager::deallocateBufferForJava(JBuffer* buffer) {
  return deallocateBuffer(buffer->get());
}

bool MmapBufferManager::deallocateBuffer(std::shared_ptr<Buffer> buffer) {
  WriterLock lock(&buffers_lock_);
  auto iter = std::find(buffers_.begin(), buffers_.end(), buffer);
  if (iter == buffers_.end()) {
    return false;
  }
  buffers_.erase(iter);
  return true;
}

void MmapBufferManager::forEachBuffer(std::function<void(Buffer&)> fn) {
  ReaderLock lock(&buffers_lock_);
  for (auto& buf : buffers_) {
    fn(*buf);
  }
}

void MmapBufferManager::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", MmapBufferManager::initHybrid),
      makeNativeMethod(
          "nativeAllocateBuffer", MmapBufferManager::allocateBufferFileForJava),
      makeNativeMethod(
          "nativeAllocateBuffer",
          MmapBufferManager::allocateBufferAnonymousForJava),
      makeNativeMethod(
          "nativeDeallocateBuffer", MmapBufferManager::deallocateBufferForJava),
  });
}

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
