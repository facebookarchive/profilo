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

#include <fbjni/fbjni.h>
#include <jni.h>

#include <profilo/mmapbuf/Buffer.h>
#include <memory>
#include <string>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace mmapbuf {

class JBuffer final : public fbjni::HybridClass<JBuffer> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/mmapbuf/Buffer;";

  static void registerNatives();

  void updateHeader(int32_t providers, int64_t long_context, int64_t trace_id);

  void updateId(const std::string& id);

  void updateFilePath(const std::string& file_path);

  void updateMemoryMappingFilename(const std::string& maps_file_path);

  fbjni::local_ref<fbjni::JString> getFilePath();

  fbjni::local_ref<fbjni::JString> getMemoryMappingFileName();

  static fbjni::local_ref<jhybridobject> makeJBuffer(
      std::weak_ptr<Buffer> ptr) {
    return newObjectCxxArgs(ptr);
  }

  std::shared_ptr<Buffer> get() {
    auto ptr = buffer_.lock();
    if (ptr == nullptr) {
      throw std::invalid_argument("Attempting to use a stale JBuffer!");
    }
    return ptr;
  }

 private:
  // Our base class needs to be able to instantiate us.
  friend class fbjni::HybridClass<JBuffer>;
  std::weak_ptr<Buffer> buffer_;

  JBuffer(std::weak_ptr<Buffer> ptr);
};

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
