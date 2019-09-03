
#include <fb/Build.h>
#include <profilo/JNILoggerHelpers.h>
#include <profilo/Logger.h>
#include <util/common.h>

namespace facebook {
namespace profilo {
namespace detail {

jint loggerWriteStandardEntry(
    JNIEnv* env,
    jobject cls,
    jint flags,
    jint type,
    jlong timestamp,
    jint tid,
    jint arg1,
    jint arg2,
    jlong arg3){
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

jint loggerWriteBytesEntry(
    JNIEnv* env,
    jobject cls,
    jint flags,
    jint type,
    jint arg1,
    jstring arg2) {
  // Android 8.0 and above can issue syscalls during Get/ReleaseStringCritical,
  // causing them to be much slower than the always-copy GetStringChars
  // version. Therefore, we use GetStringCritical
  // before 8.0 and GetStringChars after.
  static bool jniUseCritical = build::Build::getAndroidSdk() < 26;

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
  return Logger::get().writeBytes(
      static_cast<EntryType>(type), arg1, bytes, len);
}
}}}
