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

#include "NativeTraceWriterCallbacks.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace writer {

void JNativeTraceWriterCallbacks::onTraceStart(
    int64_t trace_id,
    int32_t flags) {
  static auto onTraceStartMethod =
      javaClassStatic()->getMethod<void(jlong, jint)>("onTraceWriteStart");

  onTraceStartMethod(self(), trace_id, flags);
}

void JNativeTraceWriterCallbacks::onTraceEnd(int64_t trace_id) {
  static auto onTraceEndMethod =
      javaClassStatic()->getMethod<void(jlong)>("onTraceWriteEnd");

  onTraceEndMethod(self(), trace_id);
}

void JNativeTraceWriterCallbacks::onTraceAbort(
    int64_t trace_id,
    AbortReason abortReason) {
  static auto onTraceAbortMethod =
      javaClassStatic()->getMethod<void(jlong, jint)>("onTraceWriteAbort");

  onTraceAbortMethod(self(), trace_id, static_cast<jint>(abortReason));
}

NativeTraceWriterCallbacksProxy::NativeTraceWriterCallbacksProxy(
    fbjni::alias_ref<JNativeTraceWriterCallbacks> javaCallbacks)
    : TraceCallbacks(), javaCallbacks_(fbjni::make_global(javaCallbacks)) {}

void NativeTraceWriterCallbacksProxy::onTraceStart(
    int64_t trace_id,
    int32_t flags) {
  javaCallbacks_->onTraceStart(trace_id, flags);
}

void NativeTraceWriterCallbacksProxy::onTraceEnd(int64_t trace_id) {
  javaCallbacks_->onTraceEnd(trace_id);
}

void NativeTraceWriterCallbacksProxy::onTraceAbort(
    int64_t trace_id,
    AbortReason reason) {
  javaCallbacks_->onTraceAbort(trace_id, reason);
}

} // namespace writer
} // namespace profilo
} // namespace facebook
