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
#include <profilo/logger/buffer/RingBuffer.h>

namespace fbjni = facebook::jni;

using namespace facebook::profilo::writer;

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace writer {

class MmapBufferTraceWriter : public fbjni::HybridClass<MmapBufferTraceWriter> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/mmapbuf/MmapBufferTraceWriter;";

  static fbjni::local_ref<MmapBufferTraceWriter::jhybriddata> initHybrid(
      fbjni::alias_ref<jclass>,
      std::string trace_folder,
      std::string trace_prefix,
      fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks);

  static void registerNatives();

  void nativeWriteTrace(
      std::string dump_path,
      int64_t trace_id,
      int32_t qpl_marker_id);

  // Trace re-collection from dump logic
  // Given a dump path, verifies it and re-collects the data into trace
  void writeTrace(
      std::string dump_path,
      int64_t trace_id,
      int32_t qpl_marker_id,
      uint64_t timestamp = monotonicTime());

  // For Testing
  MmapBufferTraceWriter(
      std::string trace_folder,
      std::string trace_prefix,
      std::shared_ptr<TraceCallbacks> callbacks);

 private:
  std::string trace_folder_;
  std::string trace_prefix_;
  std::shared_ptr<TraceCallbacks> callbacks_;
};

} // namespace writer
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook