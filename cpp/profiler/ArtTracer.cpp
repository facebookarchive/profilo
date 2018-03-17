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

#include "ArtTracer.h"

#include <array>
#include <mutex>
#include <pthread.h>
#include <stdexcept>

#if defined(MUSEUM_VERSION_5_1_1)
  #include <museum/5.1.1/art/runtime/fbstack_art511.h>
#elif defined(MUSEUM_VERSION_6_0_1)
  #include <museum/6.0.1/art/runtime/fbstack_art601.h>
#elif defined(MUSEUM_VERSION_7_0_0)
  #include <museum/7.0.0/art/runtime/fbstack_art700.h>
#else
  #error "Invalid ART version"
#endif

#if defined(MUSEUM_VERSION_6_0_1)
  #include <sys/limits.h> // for PTHREAD_KEYS_MAX
  #ifndef PTHREAD_KEYS_MAX
    #define PTHREAD_KEYS_MAX 1024
  #endif
#endif

#if defined(MUSEUM_VERSION_7_0_0)
  #include <museum/7.0.0/art/runtime/bionic/__get_tls.h>
  #include <museum/7.0.0/art/runtime/bionic/bionic_tls.h>
#endif

#include "profilo/Logger.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace profiler {

namespace {

using namespace museum::MUSEUM_VERSION::art::entrypoints;

#if defined(MUSEUM_VERSION_5_1_1)
  static constexpr ArtVersion kVersion = kArt511;
#elif defined(MUSEUM_VERSION_6_0_1)
  static constexpr ArtVersion kVersion = kArt601;
#elif defined(MUSEUM_VERSION_7_0_0)
  static constexpr ArtVersion kVersion = kArt700;
#endif

//
// Determine the thread local storage key for the art::Thread instance.
// Must be called from a Java thread.
// Throws std::runtime_error if no key matches.
//
// Note determineThreadInstanceTLSKey and getThreadInstanceTLSKey are not used
// on versions >= 7. See getThreadInstance.
#if !defined(MUSEUM_VERSION_7_0_0)
pthread_key_t determineThreadInstanceTLSKey() {
  auto jlThread_class = fbjni::findClassLocal("java/lang/Thread");
  auto jlThread_nativePeer = jlThread_class->getField<jlong>("nativePeer");
  auto jlThread_currentThread =
    jlThread_class->getStaticMethod<jobject()>("currentThread", "()Ljava/lang/Thread;");
  auto jlThread = jlThread_currentThread(jlThread_class);

  auto nativePeer = jlThread->getFieldValue(jlThread_nativePeer);
  void* nativeThread = reinterpret_cast<void*>(nativePeer);

#if defined(MUSEUM_VERSION_5_1_1)
  constexpr int32_t kMaxPthreadKey = 148;
  constexpr int32_t kUserPthreadKeyStart = 7;
  constexpr int32_t kKeyValidFlag = 0; // not actually in 5.1.1
#elif defined(MUSEUM_VERSION_6_0_1)
  constexpr int32_t kMaxPthreadKey = 128;
  constexpr int32_t kUserPthreadKeyStart = 0;
  constexpr int32_t kKeyValidFlag = 1 << 31; // bionic tags keys by setting the MSB
#endif

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
#endif // !defined(MUSEUM_VERSION_7_0_0)

void* getThreadInstance() {
  // See thread.cc source for the differing behavior:
  // https://android.googlesource.com/platform/art/+/android-7.0.0_r33/runtime/thread.cc#708
  #if defined(MUSEUM_VERSION_7_0_0)
    return __get_tls()[TLS_SLOT_ART_THREAD_SELF];
  #else
    return pthread_getspecific(getThreadInstanceTLSKey());
  #endif
}

bool tryInstallRuntime() {
  try {
    void* thread_instance = getThreadInstance();
    JNIEnv* env = fbjni::Environment::current();
    InstallRuntime(env, thread_instance);
    return true;
  } catch(std::exception& ex) {
    // intentionally swallowed
  }
  return false;
}

bool installRuntime() {
  static bool has_runtime = tryInstallRuntime();
  return has_runtime;
}

} // namespace

template<> ArtTracer<kVersion>::ArtTracer() {}

template<> bool ArtTracer<kVersion>::collectStack(
  ucontext_t*,
  int64_t* frames,
  uint8_t& depth,
  uint8_t max_depth) {

  void* thread = getThreadInstance();

  if (thread == nullptr) {
    return false;
  }

  JavaFrame artframes[max_depth];
  memset(artframes, 0, sizeof(JavaFrame) * max_depth);
  auto size = GetStackTrace(artframes, max_depth, thread);

  if (size >= max_depth) {
    return false;
  }

  for (size_t idx = 0; idx < size; idx++) {
    auto& frame = artframes[idx];
    frames[idx] =
      static_cast<int64_t>(frame.methodIdx) << 32 | frame.dexSignature;
  }

  depth = size;

  return true;
}

template<> void ArtTracer<kVersion>::flushStack(
  int64_t* frames,
  uint8_t depth,
  int tid,
  int64_t time_) {

  Logger::get().writeStackFrames(
    tid,
    static_cast<int64_t>(time_),
    frames,
    depth);
}

template<> void ArtTracer<kVersion>::startTracing() {
  if(!installRuntime()) {
    // The caller should have ensured compatibility before calling us.
    throw std::runtime_error("Unable to install ArtTracer runtime copy");
  }
}

template<> void ArtTracer<kVersion>::stopTracing() {
}

} // profiler
} // profilo
} // facebook
