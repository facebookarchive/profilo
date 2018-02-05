// Copyright 2004-present Facebook. All Rights Reserved.


#include <jni.h>
#include <fb/fbjni.h>
#ifdef FBJNI_BACKWARD_COMPAT
# include <jni/LocalString.h>
#else
# include <fbjni/detail/utf8.h>
#endif
#include <fb/ALog.h>
#include <fb/xplat_init.h>

#include <cstring>

#include <loom/Logger.h>
#include <loom/RingBuffer.h>
#include <loom/jni/NativeTraceWriter.h>
#include "TraceProviders.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace loom {

const char* TraceEventsType = "com/facebook/loom/core/TraceEvents";
const char* LoggerType = "com/facebook/loom/logger/Logger";

///
/// product write APIs
///

static jint loggerWrite(
    JNIEnv* env,
    jobject cls,
    jint type,
    jint arg1,
    jint arg2,
    jlong arg3
) {
  return Logger::get().write(StandardEntry {
    .id = 0,
    .type = static_cast<decltype(StandardEntry::type)>(type),
    .timestamp = monotonicTime(),
    .tid = threadID(),
    .callid = arg1,
    .matchid = arg2,
    .extra = arg3,
  });
}

static jint loggerWriteWithMonotonicTime(
    JNIEnv* env,
    jobject cls,
    jint type,
    jint arg1,
    jint arg2,
    jlong arg3,
    jlong time)
{
  return Logger::get().write(StandardEntry {
    .id = 0,
    .type = static_cast<decltype(StandardEntry::type)>(type),
    .timestamp = time,
    .tid = threadID(),
    .callid = arg1,
    .matchid = arg2,
    .extra = arg3,
  });
}

static jint loggerWriteForThread(
    JNIEnv* env,
    jobject cls,
    jint tid,
    jint type,
    jint arg1,
    jint arg2,
    jlong arg3
) {
  return Logger::get().write(StandardEntry {
    .id = 0,
    .type = static_cast<decltype(StandardEntry::type)>(type),
    .timestamp = monotonicTime(),
    .tid = tid,
    .callid = arg1,
    .matchid = arg2,
    .extra = arg3,
  });
}

static jint loggerWriteAndWakeupTraceWriter(
    fbjni::alias_ref<jobject> cls,
    writer::NativeTraceWriter* writer,
    jlong traceId,
    jint type,
    jint arg1,
    jint arg2,
    jlong arg3
) {
  if (writer == nullptr) {
    throw std::invalid_argument("writer cannot be null");
  }

  //
  // We know the buffer is initialized, NativeTraceWriter is already using it.
  // Also, currentTail is only used because Cursor is not default constructible.
  //
  LoomBuffer::Cursor cursor = RingBuffer::get().currentTail();
  jint id = Logger::get().writeAndGetCursor(StandardEntry {
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

static jint loggerWriteString(
    JNIEnv* env,
    jobject cls,
    jint type,
    jint arg1,
    jstring arg2
) {
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
    static_cast<EntryType>(type),
    arg1,
    bytes,
    len);
}

static jboolean isProviderEnabled(JNIEnv* env, jobject cls, jint provider) {
  return TraceProviders::get().isEnabled(static_cast<uint32_t>(provider));
}

static jint enabledProvidersMask(JNIEnv* env, jobject cls, jint providers) {
  return TraceProviders::get().enabledMask(static_cast<uint32_t>(providers));
}

static void enableProviders(JNIEnv* env, jobject cls, jint providers) {
  TraceProviders::get().enableProviders(static_cast<uint32_t>(providers));
}

static void disableProviders(JNIEnv* env, jobject cls, jint providers) {
  TraceProviders::get().disableProviders(static_cast<uint32_t>(providers));
}

static void clearAllProviders(JNIEnv* env, jobject cls) {
  TraceProviders::get().clearAllProviders();
}

} // namespace loom
} // namespace facebook

using namespace facebook;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return xplat::initialize(vm, [] {
    fbjni::registerNatives(
      loom::TraceEventsType,
      {
        makeNativeMethod("nativeIsEnabled", loom::isProviderEnabled),
        makeNativeMethod("nativeEnabledMask", loom::enabledProvidersMask),
        makeNativeMethod("enableProviders", loom::enableProviders),
        makeNativeMethod("disableProviders", loom::disableProviders),
        makeNativeMethod("clearAllProviders", loom::clearAllProviders),
      });

    fbjni::registerNatives(
      loom::LoggerType,
      {
        makeNativeMethod("loggerWrite", loom::loggerWrite),
        makeNativeMethod("loggerWriteWithMonotonicTime", loom::loggerWriteWithMonotonicTime),
        makeNativeMethod("loggerWriteForThread", loom::loggerWriteForThread),
        makeNativeMethod("loggerWriteString", loom::loggerWriteString),
        makeNativeMethod("loggerWriteAndWakeupTraceWriter", loom::loggerWriteAndWakeupTraceWriter),
      });

    loom::writer::NativeTraceWriter::registerNatives();
  });
}
