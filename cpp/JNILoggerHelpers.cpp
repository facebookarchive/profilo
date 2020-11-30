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

#include "JNILoggerHelpers.h"
#include <fb/Build.h>
#include <fbjni/fbjni.h>
#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <profilo/jni/NativeTraceWriter.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/JBuffer.h>
#include <util/common.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace logger_jni {

static jint loggerWriteStandardEntry(
    fbjni::alias_ref<jobject>,
    mmapbuf::JBuffer* jbuffer,
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

  return RingBuffer::get().logger().write(StandardEntry{
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
    fbjni::alias_ref<jobject>,
    mmapbuf::JBuffer* jbuffer,
    jint flags,
    jint type,
    jint arg1,
    jstring arg2) {
  // Android 8.0 and above can issue syscalls during Get/ReleaseStringCritical,
  // causing them to be much slower than the always-copy GetStringChars
  // version. Therefore, we use GetStringCritical
  // before 8.0 and GetStringChars after.
  static bool jniUseCritical = build::Build::getAndroidSdk() < 26;

  auto env = fbjni::Environment::current();
  constexpr auto kMaxJavaStringLength = 512;
  auto len = std::min(env->GetStringLength(arg2), kMaxJavaStringLength);

  uint8_t bytes[len]; // we're filtering to ASCII so one char must be one byte

  {
    // JStringUtf16Extractor is using GetStringCritical to give us raw jchar*.
    // We then filter down the wide chars to uint8_t ASCII.
    const jchar* str = nullptr;
    if (jniUseCritical) {
      str = env->GetStringCritical(arg2, nullptr);
    } else {
      str = env->GetStringChars(arg2, nullptr);
    }

    for (int i = 0; i < len; i++) {
      if (str[i] < 128) {
        bytes[i] = str[i];
      } else {
        bytes[i] = '.';
      }
    }

    if (jniUseCritical) {
      env->ReleaseStringCritical(arg2, str);
    } else {
      env->ReleaseStringChars(arg2, str);
    }
  }
  return RingBuffer::get().logger().writeBytes(
      static_cast<EntryType>(type), arg1, bytes, len);
}

static jint loggerWriteAndWakeupTraceWriter(
    fbjni::alias_ref<jobject>,
    writer::NativeTraceWriter* writer,
    mmapbuf::JBuffer* jbuffer,
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
  auto buffer = jbuffer->get();
  if (buffer == nullptr) {
    throw std::invalid_argument("buffer is null");
  }
  TraceBuffer::Cursor cursor = buffer->ringBuffer().currentTail();
  jint id = RingBuffer::get().logger().writeAndGetCursor(
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

void registerNatives() {
  fbjni::registerNatives(
      "com/facebook/profilo/logger/BufferLogger",
      {
          makeNativeMethod("writeStandardEntry", loggerWriteStandardEntry),
          makeNativeMethod("writeBytesEntry", loggerWriteBytesEntry),
          makeNativeMethod(
              "writeAndWakeupTraceWriter", loggerWriteAndWakeupTraceWriter),
      });
}

} // namespace logger_jni
} // namespace profilo
} // namespace facebook
