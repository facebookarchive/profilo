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
#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <profilo/jni/NativeTraceWriter.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/JBuffer.h>

namespace facebook {
namespace profilo {
namespace logger_jni {

static Logger& getBufferLoggerOrGlobal(mmapbuf::JBuffer* jbuffer) {
  Logger* logger = nullptr;
  if (jbuffer != nullptr) {
    logger = &jbuffer->get()->logger();
  } else {
    logger = &RingBuffer::get().logger();
  }
  return *logger;
}

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
  return writeStandardEntryFromJNI(
      getBufferLoggerOrGlobal(jbuffer),
      flags,
      type,
      timestamp,
      tid,
      arg1,
      arg2,
      arg3);
}

static jint loggerWriteBytesEntry(
    fbjni::alias_ref<jobject>,
    mmapbuf::JBuffer* jbuffer,
    jint flags,
    jint type,
    jint arg1,
    jstring arg2) {
  return writeBytesEntryFromJNI(
      getBufferLoggerOrGlobal(jbuffer), flags, type, arg1, arg2);
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
  jint id = buffer->logger().writeAndGetCursor(
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
