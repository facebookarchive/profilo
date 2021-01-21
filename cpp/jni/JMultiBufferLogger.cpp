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

#include "JMultiBufferLogger.h"
#include <linker/locks.h>
#include <profilo/JNILoggerHelpers.h>
#include <util/common.h>

namespace facebook {
namespace profilo {
namespace logger {

using namespace logger_jni;

void JMultiBufferLogger::initHybrid(fbjni::alias_ref<jhybridobject> obj) {
  setCxxInstance(obj);
}

void JMultiBufferLogger::addBuffer(JBuffer* buffer) {
  logger_.addBuffer(buffer->get());
}

void JMultiBufferLogger::removeBuffer(JBuffer* buffer) {
  logger_.removeBuffer(buffer->get());
}

MultiBufferLogger& JMultiBufferLogger::nativeInstance() {
  return logger_;
}

jint JMultiBufferLogger::writeStandardEntry(
    jint flags,
    jint type,
    jlong timestamp,
    jint tid,
    jint arg1,
    jint arg2,
    jlong arg3) {
  return writeStandardEntryFromJNI(
      nativeInstance(), flags, type, timestamp, tid, arg1, arg2, arg3);
}

jint JMultiBufferLogger::writeBytesEntry(
    jint flags,
    jint type,
    jint arg1,
    jstring arg2) {
  return writeBytesEntryFromJNI(nativeInstance(), flags, type, arg1, arg2);
}

void JMultiBufferLogger::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", JMultiBufferLogger::initHybrid),
      makeNativeMethod("nativeAddBuffer", JMultiBufferLogger::addBuffer),
      makeNativeMethod("nativeRemoveBuffer", JMultiBufferLogger::removeBuffer),
      makeNativeMethod(
          "nativeWriteStandardEntry", JMultiBufferLogger::writeStandardEntry),
      makeNativeMethod("nativeWriteBytesEntry", JMultiBufferLogger::writeBytesEntry),
  });
}
} // namespace logger
} // namespace profilo
} // namespace facebook
