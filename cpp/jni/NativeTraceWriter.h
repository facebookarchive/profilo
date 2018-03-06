// Copyright 2004-present Facebook. All Rights Reserved.

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
    int bufferSize,
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
    size_t bufferSize,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks
  );

  std::shared_ptr<TraceCallbacks> callbacks_;
  writer::TraceWriter writer_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
