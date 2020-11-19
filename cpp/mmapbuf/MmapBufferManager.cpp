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
#include <system_error>

#include <fb/log.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/header/MmapBufferHeader.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {

bool MmapBufferManager::allocateBuffer(
    int32_t buffer_size,
    const std::string& path,
    int32_t version_code,
    int64_t config_id) {
  try {
    buffer_ = std::make_shared<Buffer>(path, (size_t)buffer_size);
  } catch (std::system_error& ex) {
    FBLOGE("%s", ex.what());
    return false;
  }

  buffer_->prefix->header.bufferVersion = RingBuffer::kVersion;
  buffer_->prefix->header.size = (size_t)buffer_size;
  buffer_->prefix->header.versionCode = version_code;
  buffer_->prefix->header.configId = config_id;

  // Pass the buffer to the global singleton
  RingBuffer::init(*buffer_);

  return true;
}

void MmapBufferManager::deallocateBuffer() {
  RingBuffer::destroy();
  buffer_ = nullptr;
}

void MmapBufferManager::updateHeader(
    int32_t providers,
    int64_t long_context,
    int64_t trace_id) {
  if (buffer_ == nullptr) {
    return;
  }
  buffer_->prefix->header.providers = providers;
  buffer_->prefix->header.longContext = long_context;
  buffer_->prefix->header.traceId = trace_id;
}

void MmapBufferManager::updateId(const std::string& id) {
  if (buffer_ == nullptr) {
    return;
  }
  // Compare and if session id has not changed exit
  auto sz = std::min(
      id.size(), (size_t)header::MmapBufferHeader::kSessionIdLength - 1);
  id.copy(buffer_->prefix->header.sessionId, sz);
  buffer_->prefix->header.sessionId[sz] = 0;
}

void MmapBufferManager::updateFilePath(const std::string& file_path) {
  if (buffer_ == nullptr) {
    return;
  }
  buffer_->rename(file_path);
}

void MmapBufferManager::updateMemoryMappingFilename(
    const std::string& maps_filename) {
  if (buffer_ == nullptr) {
    return;
  }
  // Compare and if session id has not changed exit
  auto sz = std::min(
      maps_filename.size(),
      (size_t)header::MmapBufferHeader::kMemoryMapsFilenameLength - 1);
  maps_filename.copy(buffer_->prefix->header.memoryMapsFilename, sz);
  buffer_->prefix->header.memoryMapsFilename[sz] = 0;
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
      makeNativeMethod("nativeUpdateId", MmapBufferManager::updateId),
      makeNativeMethod(
          "nativeUpdateFilePath", MmapBufferManager::updateFilePath),
      makeNativeMethod(
          "nativeUpdateMemoryMappingFilename",
          MmapBufferManager::updateMemoryMappingFilename),
  });
}

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
