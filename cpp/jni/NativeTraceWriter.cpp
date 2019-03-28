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

#include "NativeTraceWriter.h"

#include <errno.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sstream>
#include <system_error>

using facebook::jni::alias_ref;
using facebook::jni::local_ref;

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace writer {

namespace {

//
// An object that takes ownership of the Java callbacks object and also
// delegates all TraceCallbacks calls to it.
//
struct NativeTraceWriterCallbacksProxy : public TraceCallbacks {
  NativeTraceWriterCallbacksProxy(
      fbjni::alias_ref<JNativeTraceWriterCallbacks> javaCallbacks)
      : TraceCallbacks(), javaCallbacks_(fbjni::make_global(javaCallbacks)) {}

  virtual void onTraceStart(int64_t trace_id, int32_t flags, std::string file)
      override {
    javaCallbacks_->onTraceStart(trace_id, flags, file);
  }

  virtual void onTraceEnd(int64_t trace_id, uint32_t crc) override {
    javaCallbacks_->onTraceEnd(trace_id, crc);
  }

  virtual void onTraceAbort(int64_t trace_id, AbortReason reason) override {
    javaCallbacks_->onTraceAbort(trace_id, reason);
  }

 private:
  fbjni::global_ref<JNativeTraceWriterCallbacks> javaCallbacks_;
};

} // anonymous namespace

void JNativeTraceWriterCallbacks::onTraceStart(
    int64_t trace_id,
    int32_t flags,
    std::string file) {
  static auto onTraceStartMethod =
      javaClassStatic()->getMethod<void(jlong, jint, std::string)>(
          "onTraceWriteStart");

  onTraceStartMethod(self(), trace_id, flags, file);
}

void JNativeTraceWriterCallbacks::onTraceEnd(int64_t trace_id, uint32_t crc) {
  static auto onTraceEndMethod =
      javaClassStatic()->getMethod<void(jlong, jint)>("onTraceWriteEnd");

  onTraceEndMethod(self(), trace_id, crc);
}

void JNativeTraceWriterCallbacks::onTraceAbort(
    int64_t trace_id,
    AbortReason abortReason) {
  static auto onTraceAbortMethod =
      javaClassStatic()->getMethod<void(jlong, jint)>("onTraceWriteAbort");

  onTraceAbortMethod(self(), trace_id, static_cast<jint>(abortReason));
}

namespace {

std::vector<std::pair<std::string, std::string>> calculateHeaders() {
  auto result = std::vector<std::pair<std::string, std::string>>();
  result.reserve(4);

  {
    std::stringstream ss;
    ss << getpid();
    result.push_back(std::make_pair("pid", ss.str()));
  }

  {
    struct utsname name {};
    if (uname(&name)) {
      throw std::system_error(
          errno, std::system_category(), "could not uname(2)");
    }

    result.push_back(std::make_pair("arch", std::string(name.machine)));
  }

  {
    char prop_value[PROP_VALUE_MAX]{};
    if (__system_property_get("ro.build.version.release", prop_value) > 0) {
      std::stringstream ss;
      ss << "Android" << prop_value;

      result.push_back(std::make_pair("os", ss.str()));
    }
  }

  return result;
}

} // namespace

NativeTraceWriter::NativeTraceWriter(
    std::string trace_folder,
    std::string trace_prefix,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks)
    :

      callbacks_(std::make_shared<NativeTraceWriterCallbacksProxy>(callbacks)),
      writer_(
          std::move(trace_folder),
          std::move(trace_prefix),
          RingBuffer::get(),
          callbacks_,
          calculateHeaders()) {}

void NativeTraceWriter::loop() {
  writer_.loop();
}

void NativeTraceWriter::submit(TraceBuffer::Cursor cursor, int64_t trace_id) {
  writer_.submit(cursor, trace_id);
}

local_ref<NativeTraceWriter::jhybriddata> NativeTraceWriter::initHybrid(
    alias_ref<jclass>,
    std::string trace_folder,
    std::string trace_prefix,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks) {
  return makeCxxInstance(trace_folder, trace_prefix, callbacks);
}

void NativeTraceWriter::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", NativeTraceWriter::initHybrid),
      makeNativeMethod("loop", NativeTraceWriter::loop),
  });
}

} // namespace writer
} // namespace profilo
} // namespace facebook
