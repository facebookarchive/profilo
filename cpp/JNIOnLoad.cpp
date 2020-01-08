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

#include <fb/ALog.h>
#include <fb/xplat_init.h>
#include <fbjni/detail/utf8.h>
#include <fbjni/fbjni.h>
#include <jni.h>

#include <cstring>
#include <vector>

#include <profilo/JNILoggerHelpers.h>
#include <profilo/Logger.h>
#include <profilo/jni/NativeTraceWriter.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include "TraceProviders.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {

const char* TraceEventsType = "com/facebook/profilo/core/TraceEvents";
const char* LoggerType = "com/facebook/profilo/logger/Logger";

///
/// product write APIs
///

static jint loggerWriteAndWakeupTraceWriter(
    fbjni::alias_ref<jobject> cls,
    writer::NativeTraceWriter* writer,
    jlong traceId,
    jint type,
    jint arg1,
    jint arg2,
    jlong arg3) {
  if (writer == nullptr) {
    throw std::invalid_argument("writer cannot be null");
  }

  //
  // We know the buffer is initialized, NativeTraceWriter is already using it.
  // Also, currentTail is only used because Cursor is not default constructible.
  //
  TraceBuffer::Cursor cursor = RingBuffer::get().currentTail();
  jint id = Logger::get().writeAndGetCursor(
      StandardEntry{
          .id = 0,
          .type = static_cast<decltype(StandardEntry::type)>(type),
          .timestamp = monotonicTime(),
          .tid = threadID(),
          .callid = arg1,
          .matchid = arg2,
          .extra = arg3,
      },
      cursor);

  writer->submit(cursor, traceId);
  return id;
}

static void stopTraceWriter(
    fbjni::alias_ref<jobject> cls,
    writer::NativeTraceWriter* writer) {
  if (writer == nullptr) {
    throw std::invalid_argument("writer cannot be null (teardown)");
  }

  TraceBuffer::Cursor cursor = RingBuffer::get().currentTail();
  writer->submit(cursor, writer::TraceWriter::kStopLoopTraceID);
}

static jint enableProviders(JNIEnv* env, jobject cls, jint providers) {
  return TraceProviders::get().enableProviders(
      static_cast<uint32_t>(providers));
}

static jint disableProviders(JNIEnv* env, jobject cls, jint providers) {
  return TraceProviders::get().disableProviders(
      static_cast<uint32_t>(providers));
}

static void clearAllProviders(JNIEnv* env, jobject cls) {
  TraceProviders::get().clearAllProviders();
}

static void initProviderNames(
    fbjni::alias_ref<jobject> cls,
    fbjni::alias_ref<fbjni::JArrayInt> provider_ids,
    fbjni::alias_ref<fbjni::jtypeArray<jstring>> provider_names) {
  auto provider_ids_array = provider_ids->pin();
  auto size = provider_ids_array.size();
  std::vector<ProviderEntry> provider_names_vec;
  for (int i = 0; i < size; i++) {
    provider_names_vec.emplace_back(
        provider_names->getElement(i)->toStdString(), provider_ids_array[i]);
  }
  TraceProviders::get().initProviderNames(std::move(provider_names_vec));
}

static void initRingBuffer(JNIEnv* env, jobject cls, jint size) {
  RingBuffer::init(size);
}

} // namespace profilo
} // namespace facebook

using namespace facebook;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return xplat::initialize(vm, [] {
    fbjni::registerNatives(
        profilo::TraceEventsType,
        {
            makeNativeMethod("nativeEnableProviders", profilo::enableProviders),
            makeNativeMethod(
                "nativeDisableProviders", profilo::disableProviders),
            makeNativeMethod(
                "nativeClearAllProviders", profilo::clearAllProviders),
            makeNativeMethod(
                "nativeInitProviderNames", profilo::initProviderNames),
        });

    fbjni::registerNatives(
        profilo::LoggerType,
        {
            makeNativeMethod(
                "loggerWriteStandardEntry",
                profilo::detail::loggerWriteStandardEntry),
            makeNativeMethod(
                "loggerWriteBytesEntry",
                profilo::detail::loggerWriteBytesEntry),
            makeNativeMethod(
                "loggerWriteAndWakeupTraceWriter",
                profilo::loggerWriteAndWakeupTraceWriter),
            makeNativeMethod("nativeInitRingBuffer", profilo::initRingBuffer),
            makeNativeMethod("stopTraceWriter", profilo::stopTraceWriter),
        });

    profilo::writer::NativeTraceWriter::registerNatives();
  });
}
