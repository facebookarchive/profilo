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

#include <profilo/mmapbuf/header/MmapBufferHeader.h>

#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <jni.h>

#include <atomic>
#include <string>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace mmapbuf {

using namespace facebook::profilo::mmapbuf::header;

class MmapBufferManager : public fbjni::HybridClass<MmapBufferManager> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/mmapbuf/MmapBufferManager;";

  static fbjni::local_ref<MmapBufferManager::jhybriddata> initHybrid(
      fbjni::alias_ref<jclass>);

  //
  // Allocates TraceBuffer according to the passed parameters in a file.
  // Returns true if the buffer was allocated and false otherwise.
  //
  bool allocateBuffer(
      int32_t buffer_slots_size,
      const std::string& path,
      int32_t version_code,
      int64_t config_id);

  //
  // De-allocates previously allocated buffer and deletes the file.
  //
  void deallocateBuffer();

  // Updates current tracing state
  void updateHeader(
      int32_t providers,
      int32_t long_context,
      int64_t normal_trace_id,
      int64_t memory_trace_id);

  void updateId(const std::string& id);

  void updateFilePath(const std::string& file_path);

  static void registerNatives();

  explicit MmapBufferManager();

 private:
  std::string path_;
  size_t size_;
  std::atomic<MmapBufferPrefix*> buffer_prefix_;

  friend class MmapBufferManagerTestAccessor;
};

} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
