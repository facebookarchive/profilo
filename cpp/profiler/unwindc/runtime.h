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

#pragma once

#include <cstddef>
#include <fb/log.h>
#include <stdint.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fbjni/fbjni.h>

namespace fbjni = facebook::jni;

struct string_t {
  const char* data;
  size_t length;
};

typedef bool (*unwind_callback_t)(uintptr_t, void*);

static void* find_runtime_instance() {
  auto handle = dlopen("libart.so", RTLD_NOW|RTLD_GLOBAL);
  if (handle == nullptr) {
    FBLOGE("Need libart.so");
    return nullptr;
  }
  auto result = dlsym(handle, "_ZN3art7Runtime9instance_E");
  dlclose(handle);
  void** ptr = reinterpret_cast<void**>(result);
  return *ptr;
}

uintptr_t get_runtime() {
  static void* runtime = find_runtime_instance();
  return reinterpret_cast<uintptr_t>(runtime);
}

pthread_key_t determineThreadInstanceTLSKey() {
  auto jlThread_class = fbjni::findClassLocal("java/lang/Thread");
  auto jlThread_nativePeer = jlThread_class->getField<jlong>("nativePeer");
  auto jlThread_currentThread =
    jlThread_class->getStaticMethod<jobject()>("currentThread", "()Ljava/lang/Thread;");
  auto jlThread = jlThread_currentThread(jlThread_class);

  auto nativePeer = jlThread->getFieldValue(jlThread_nativePeer);
  void* nativeThread = reinterpret_cast<void*>(nativePeer);

  constexpr int32_t kMaxPthreadKey = 128;
  constexpr int32_t kUserPthreadKeyStart = 0;
  constexpr int32_t kKeyValidFlag = 1 << 31; // bionic tags keys by setting the MSB

  for (pthread_key_t i = kUserPthreadKeyStart; i < kMaxPthreadKey; i++) {
    pthread_key_t tagged = i | kKeyValidFlag;
    if (pthread_getspecific(tagged) == nativeThread) {
      return tagged;
    }
  }
  throw std::runtime_error("Cannot determine thread instance TLS key");
}

pthread_key_t getThreadInstanceTLSKey() {
  static pthread_key_t key = determineThreadInstanceTLSKey();
  return key;
}

void* getThreadInstance() {
  return pthread_getspecific(getThreadInstanceTLSKey());
}

uintptr_t get_art_thread() {
  return reinterpret_cast<uintptr_t>(getThreadInstance());
}

__attribute__((always_inline))
inline uint32_t CountShortyRefs(string_t shorty) {
  uint32_t result = 0;
  for (size_t i = 0; i < shorty.length; i++) {
    if (shorty.data[i] == 'L') {
      result++;
    }
  }
  return result;
}

__attribute__((always_inline))
inline string_t String(string_t data) {
  return data;
}

__attribute__((always_inline))
inline string_t String(uintptr_t ptr, const char*, const char*, uint32_t length) {
  return string_t{
    .data = (const char*) ptr,
    .length = length
  };
}

__attribute__((always_inline))
inline uint64_t GetMethodTraceId(uint32_t dex_id, uint32_t method_id) {
  return (static_cast<uint64_t>(method_id) << 32) | dex_id;
}

__attribute__((always_inline))
inline uint8_t Read1(uintptr_t addr) {
  return *reinterpret_cast<uint8_t*>(addr);
}

__attribute__((always_inline))
inline uint16_t Read2(uintptr_t addr) {
  return *reinterpret_cast<uint16_t*>(addr);
}

__attribute__((always_inline))
inline uint32_t Read4(uintptr_t addr) {
  return *reinterpret_cast<uint32_t*>(addr);
}

__attribute__((always_inline))
inline uint64_t Read8(uintptr_t addr) {
  return *reinterpret_cast<uint64_t*>(addr);
}

__attribute__((always_inline))
inline uintptr_t AccessField(uintptr_t addr, uint32_t offset) {
  return addr + offset;
}

__attribute__((always_inline))
inline uintptr_t AccessArrayItem(uintptr_t addr, uint32_t item_size, uint32_t item) {
  return addr + (item_size * item);
}
