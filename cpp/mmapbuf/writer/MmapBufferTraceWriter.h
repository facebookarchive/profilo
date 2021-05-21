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

#pragma once

#include <unistd.h>
#include <string>

#include <fbjni/fbjni.h>

#include <profilo/Logger.h>
#include <profilo/jni/NativeTraceWriterCallbacks.h>
#include <profilo/util/common.h>

#include <profilo/mmapbuf/writer/BufferFileMapHolder.h>

namespace fbjni = facebook::jni;

using namespace facebook::profilo::writer;

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace writer {

class MmapBufferTraceWriter : public fbjni::HybridClass<MmapBufferTraceWriter> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/mmapbuf/writer/MmapBufferTraceWriter;";

  static fbjni::local_ref<MmapBufferTraceWriter::jhybriddata> initHybrid(
      fbjni::alias_ref<jclass>);

  static void registerNatives();

  int64_t nativeInitAndVerify(const std::string& dump_path);

  void nativeWriteTrace(
      const std::string& type,
      const std::string& trace_folder,
      const std::string& trace_prefix,
      int32_t trace_flags,
      fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks);

  // Trace re-collection from dump logic
  // Given a dump path, verifies it and re-collects the data into trace
  void writeTrace(
      const std::string& type,
      const std::string& trace_folder,
      const std::string& trace_prefix,
      int32_t trace_flags,
      std::shared_ptr<TraceCallbacks> callbacks,
      uint64_t timestamp = monotonicTime());

 private:
  std::unique_ptr<BufferFileMapHolder> bufferMapHolder_;
  std::string dump_path_;
  int64_t trace_id_ = 0;
};

} // namespace writer
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
