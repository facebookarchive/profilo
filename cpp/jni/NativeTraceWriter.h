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

#include <profilo/writer/TraceWriter.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace writer {

struct JNativeTraceWriterCallbacks:
  public fbjni::JavaClass<JNativeTraceWriterCallbacks> {

  static constexpr const char* kJavaDescriptor =
    "Lcom/facebook/profilo/writer/NativeTraceWriterCallbacks;";

  void onTraceStart(int64_t trace_id, int32_t flags, std::string file);
  void onTraceEnd(int64_t trace_id, uint32_t crc);
  void onTraceAbort(int64_t trace_id, AbortReason abortReason);
};

class NativeTraceWriter : public fbjni::HybridClass<NativeTraceWriter> {
 public:
  constexpr static auto kJavaDescriptor =
    "Lcom/facebook/profilo/writer/NativeTraceWriter;";

  static fbjni::local_ref<jhybriddata> initHybrid(
    fbjni::alias_ref<jclass>,
    std::string trace_folder,
    std::string trace_prefix,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks
  );

  static void registerNatives();

  void loop();

  void submit(TraceBuffer::Cursor cursor, int64_t trace_id);

 private:
  friend HybridBase;

  NativeTraceWriter(
    std::string trace_folder,
    std::string trace_prefix,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks
  );

  std::shared_ptr<TraceCallbacks> callbacks_;
  writer::TraceWriter writer_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
