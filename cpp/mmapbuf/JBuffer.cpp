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

#include "JBuffer.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace mmapbuf {

JBuffer::JBuffer(std::weak_ptr<Buffer> buffer) : buffer_(std::move(buffer)) {}

void JBuffer::updateHeader(
    int32_t providers,
    int64_t long_context,
    int64_t trace_id,
    int64_t config_id) {
  auto buffer = buffer_.lock();
  if (buffer == nullptr) {
    return;
  }
  buffer->prefix->header.providers = providers;
  buffer->prefix->header.longContext = long_context;
  buffer->prefix->header.traceId = trace_id;
  buffer->prefix->header.configId = config_id;
}

void JBuffer::updateId(const std::string& id) {
  auto buffer = buffer_.lock();
  if (buffer == nullptr) {
    return;
  }
  // Compare and if session id has not changed exit
  auto sz = std::min(
      id.size(), (size_t)header::MmapBufferHeader::kSessionIdLength - 1);
  id.copy(buffer->prefix->header.sessionId, sz);
  buffer->prefix->header.sessionId[sz] = 0;
}

void JBuffer::updateFilePath(const std::string& file_path) {
  auto buffer = buffer_.lock();
  if (buffer == nullptr) {
    return;
  }
  buffer->rename(file_path);
}

void JBuffer::updateMemoryMappingFilePath(const std::string& maps_file_path) {
  auto buffer = buffer_.lock();
  if (buffer == nullptr) {
    return;
  }
  // Compare and if session id has not changed exit
  auto sz = std::min(
      maps_file_path.size(),
      (size_t)header::MmapBufferHeader::kMemoryMapsFilePathLength - 1);
  maps_file_path.copy(buffer->prefix->header.memoryMapsFilePath, sz);
  buffer->prefix->header.memoryMapsFilePath[sz] = 0;
}

fbjni::local_ref<fbjni::JString> JBuffer::getFilePath() {
  auto buffer = buffer_.lock();
  if (buffer == nullptr || buffer->path.empty()) {
    return nullptr;
  }
  return fbjni::make_jstring(buffer->path);
}

fbjni::local_ref<fbjni::JString> JBuffer::getMemoryMappingFilePath() {
  auto buffer = buffer_.lock();
  if (buffer == nullptr) {
    return nullptr;
  }
  auto memoryMappingFile =
      std::string{buffer->prefix->header.memoryMapsFilePath};
  if (memoryMappingFile.empty()) {
    return nullptr;
  }
  return fbjni::make_jstring(memoryMappingFile);
}

void JBuffer::registerNatives() {
  registerHybrid({
      makeNativeMethod("updateHeader", JBuffer::updateHeader),
      makeNativeMethod("nativeUpdateId", JBuffer::updateId),
      makeNativeMethod("updateFilePath", JBuffer::updateFilePath),
      makeNativeMethod(
          "updateMemoryMappingFilePath", JBuffer::updateMemoryMappingFilePath),
      makeNativeMethod(
          "getMemoryMappingFilePath", JBuffer::getMemoryMappingFilePath),
      makeNativeMethod("getFilePath", JBuffer::getFilePath),
  });
}
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
