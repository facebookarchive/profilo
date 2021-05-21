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

#include <profilo/LogEntry.h>
#include <profilo/entries/Entry.h>
#include <profilo/util/common.h>
#include <cstdint>

#ifdef __ANDROID__
#include <fb/Build.h>
#endif

#include <fbjni/fbjni.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {

using namespace entries;

namespace logger_jni {

// These flags should match the ones from Logger.java
static constexpr uint32_t FILL_TIMESTAMP = 1 << 1;
static constexpr uint32_t FILL_TID = 1 << 2;

template <typename LoggerLike>
jint writeStandardEntryFromJNI(
    LoggerLike& loggerLike,
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

  StandardEntry entry{
      .id = 0,
      .type = static_cast<decltype(StandardEntry::type)>(type),
      .timestamp = timestamp,
      .tid = tid,
      .callid = arg1,
      .matchid = arg2,
      .extra = arg3,
  };
  return loggerLike.write(entry);
}

template <typename LoggerLike>
jint writeBytesEntryFromJNI(
    LoggerLike& loggerLike,
    jint flags,
    jint type,
    jint arg1,
    jstring arg2) {
  if (arg2 == nullptr) {
    constexpr uint8_t bytes[] = "null";
    constexpr int bytesLen = 4;
    return loggerLike.writeBytes(
        static_cast<EntryType>(type), arg1, bytes, bytesLen);
  }

  // Android 8.0 and above can issue syscalls during Get/ReleaseStringCritical,
  // causing them to be much slower than the always-copy GetStringChars
  // version. Therefore, we use GetStringCritical
  // before 8.0 and GetStringChars after.
#ifdef __ANDROID__
  static bool jniUseCritical = build::Build::getAndroidSdk() < 26;
#else
  static bool jniUseCritical = true;
#endif

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
  return loggerLike.writeBytes(static_cast<EntryType>(type), arg1, bytes, len);
}

void registerNatives();

} // namespace logger_jni
} // namespace profilo
} // namespace facebook
