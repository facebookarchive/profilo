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
#include <unordered_map>

#include <profilo/Logger.h>
#include <profilo/RingBuffer.h>
#include <profilo/jni/NativeTraceWriter.h>
#include "TraceProviders.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {

const char* TraceEventsType = "com/facebook/profilo/core/TraceEvents";
const char* LoggerType = "com/facebook/profilo/logger/Logger";

///
/// product write APIs
///

// These flags should match the ones from Logger.java
static constexpr uint32_t FILL_TIMESTAMP = 1 << 1;
static constexpr uint32_t FILL_TID = 1 << 2;

static jint loggerWriteStandardEntry(
    JNIEnv* env,
    jobject cls,
    jint flags,
    jint type,
    jlong timestamp,
    jint tid,
    jint arg1,
    jint arg2,
    jlong arg3) {
  if (flags & FILL_TIMESTAMP) {
    timestamp = monotonicTime();
  }

  if (flags & FILL_TID) {
    tid = threadID();
  }

  return Logger::get().write(StandardEntry{
      .id = 0,
      .type = static_cast<decltype(StandardEntry::type)>(type),
      .timestamp = timestamp,
      .tid = tid,
      .callid = arg1,
      .matchid = arg2,
      .extra = arg3,
  });
}

static jint loggerWriteBytesEntry(
    JNIEnv* env,
    jobject cls,
    jint flags,
    jint type,
    jint arg1,
    jstring arg2) {
  const auto kMaxJavaStringLength = 512;
  auto len = std::min(env->GetStringLength(arg2), kMaxJavaStringLength);
  uint8_t bytes[len]; // we're filtering to ASCII so one char must be one byte

  {
    // JStringUtf16Extractor is using GetStringCritical to give us raw jchar*.
    // We then filter down the wide chars to uint8_t ASCII.
    auto extract = fbjni::JStringUtf16Extractor(env, arg2);
    const jchar* str = extract.chars();
    for (int i = 0; i < len; i++) {
      if (str[i] < 128) {
        bytes[i] = str[i];
      } else {
        bytes[i] = '.';
      }
    }
  }
  return Logger::get().writeBytes(
      static_cast<EntryType>(type), arg1, bytes, len);
}

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
  std::unordered_map<std::string, uint32_t> provider_names_map;
  for (int i = 0; i < size; i++) {
    provider_names_map.emplace(
        provider_names->getElement(i)->toStdString(), provider_ids_array[i]);
  }
  TraceProviders::get().initProviderNames(std::move(provider_names_map));
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
                "loggerWriteStandardEntry", profilo::loggerWriteStandardEntry),
            makeNativeMethod(
                "loggerWriteBytesEntry", profilo::loggerWriteBytesEntry),
            makeNativeMethod(
                "loggerWriteAndWakeupTraceWriter",
                profilo::loggerWriteAndWakeupTraceWriter),
            makeNativeMethod("nativeInitRingBuffer", profilo::initRingBuffer),
        });

    profilo::writer::NativeTraceWriter::registerNatives();
  });
}
