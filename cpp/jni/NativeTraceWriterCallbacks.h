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

struct JNativeTraceWriterCallbacks
    : public fbjni::JavaClass<JNativeTraceWriterCallbacks> {
  static constexpr const char* kJavaDescriptor =
      "Lcom/facebook/profilo/writer/NativeTraceWriterCallbacks;";

  void onTraceStart(int64_t trace_id, int32_t flags);
  void onTraceEnd(int64_t trace_id);
  void onTraceAbort(int64_t trace_id, AbortReason abortReason);
};

//
// An object that takes ownership of the Java callbacks object and also
// delegates all TraceCallbacks calls to it.
//
struct NativeTraceWriterCallbacksProxy : public TraceCallbacks {
  NativeTraceWriterCallbacksProxy(
      fbjni::alias_ref<JNativeTraceWriterCallbacks> javaCallbacks);

  virtual void onTraceStart(int64_t trace_id, int32_t flags) override;

  virtual void onTraceEnd(int64_t trace_id) override;

  virtual void onTraceAbort(int64_t trace_id, AbortReason reason) override;

 private:
  fbjni::global_ref<JNativeTraceWriterCallbacks> javaCallbacks_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
