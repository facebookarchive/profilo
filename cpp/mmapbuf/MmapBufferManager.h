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

#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <jni.h>

#include <profilo/mmapbuf/Buffer.h>
#include <profilo/mmapbuf/JBuffer.h>
#include <memory>
#include <string>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace mmapbuf {

class MmapBufferManager : public fbjni::HybridClass<MmapBufferManager> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/mmapbuf/MmapBufferManager;";

  static fbjni::local_ref<MmapBufferManager::jhybriddata> initHybrid(
      fbjni::alias_ref<jclass>);
  //
  // Allocates TraceBuffer according to the passed parameters in a file.
  // Returns a non-null reference if successful, nullptr if not.
  //
  std::shared_ptr<Buffer> allocateBufferAnonymous(int32_t buffer_slots_size);

  fbjni::local_ref<JBuffer::javaobject> allocateBufferAnonymousForJava(
      int32_t buffer_slots_size);

  //
  // Allocates TraceBuffer according to the passed parameters in a file.
  // Returns a non-null reference if successful, nullptr if not.
  //
  std::shared_ptr<Buffer> allocateBufferFile(
      int32_t buffer_slots_size,
      const std::string& path,
      int32_t version_code,
      int64_t config_id);

  fbjni::local_ref<JBuffer::javaobject> allocateBufferFileForJava(
      int32_t buffer_slots_size,
      const std::string& path,
      int32_t version_code,
      int64_t config_id);

  //
  // De-allocates previously allocated buffer and deletes the file.
  //
  void deallocateBuffer();

  static void registerNatives();

 private:
  std::shared_ptr<Buffer> buffer_ = nullptr;

  friend class MmapBufferManagerTestAccessor;
};

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
