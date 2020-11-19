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
#include <profilo/mmapbuf/JBuffer.h>
#include <util/common.h>
#include "TraceProviders.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {

///
/// product write APIs
///

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

static void refreshProviderNames(
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

} // namespace profilo
} // namespace facebook

using namespace facebook;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return xplat::initialize(vm, [] {
    fbjni::registerNatives(
        "com/facebook/profilo/core/TraceEvents",
        {
            makeNativeMethod("nativeEnableProviders", profilo::enableProviders),
            makeNativeMethod(
                "nativeDisableProviders", profilo::disableProviders),
            makeNativeMethod(
                "nativeClearAllProviders", profilo::clearAllProviders),
            makeNativeMethod(
                "nativeRefreshProviderNames", profilo::refreshProviderNames),
        });

    profilo::writer::NativeTraceWriter::registerNatives();
    profilo::logger_jni::registerNatives();
  });
}
