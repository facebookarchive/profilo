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

#include "ArtCompatibility.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fbjni/fbjni.h>
#include <fb/log.h>
#include <ios>
#include <thread>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(MUSEUM_VERSION_5_1_1)
  #include <museum/5.1.1/art/runtime/fbstack_art511.h>
#elif defined(MUSEUM_VERSION_6_0_1)
  #include <museum/6.0.1/art/runtime/fbstack_art601.h>
#elif defined(MUSEUM_VERSION_7_0_0)
  #include <museum/7.0.0/art/runtime/fbstack_art700.h>
#else
  #error "Invalid ART version"
#endif

#include <forkjail/ForkJail.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace artcompat {

namespace {

using namespace museum::MUSEUM_VERSION::art::entrypoints;

static const int kStackSize = 128;

fbjni::local_ref<jclass> getThreadClass() {
  return fbjni::findClassLocal("java/lang/Thread");
}

std::vector<std::set<uint32_t>> getJavaStackTrace(fbjni::alias_ref<jobject> thread) {
  auto jlThread_class = getThreadClass();
  auto jlThread_getStackTrace = jlThread_class->getMethod<fbjni::jtypeArray<jobject>()>("getStackTrace", "()[Ljava/lang/StackTraceElement;");

  auto jlStackTraceElement_class = fbjni::findClassLocal("java/lang/StackTraceElement");
  auto jlStackTraceElement_getClassName = jlStackTraceElement_class->getMethod<jstring()>("getClassName");
  auto jlStackTraceElement_getMethodName = jlStackTraceElement_class->getMethod<jstring()>("getMethodName");

  auto jlClass_class = fbjni::findClassLocal("java/lang/Class");
  auto jlClass_getDeclaredMethods = jlClass_class->getMethod<fbjni::jtypeArray<jobject>()>("getDeclaredMethods", "()[Ljava/lang/reflect/Method;");

  auto jlrMethod_class = fbjni::findClassLocal("java/lang/reflect/Method");
  auto jlrMethod_getName = jlrMethod_class->getMethod<jstring()>("getName");

#if defined(MUSEUM_VERSION_5_1_1)
  auto jlrArtMethod_class = fbjni::findClassLocal("java/lang/reflect/ArtMethod");
  auto jlrMethod_getArtMethod = jlrMethod_class->getMethod<jobject()>("getArtMethod","()Ljava/lang/reflect/ArtMethod;");
  auto jlrArtMethod_dexMethodIndex = jlrArtMethod_class->getField<jint>("dexMethodIndex");
#elif defined(MUSEUM_VERSION_6_0_1) || defined(MUSEUM_VERSION_7_0_0)
  auto jlrMethod_dexMethodIndex = jlrMethod_class->getField<jint>("dexMethodIndex");
#endif

  auto stacktrace = jlThread_getStackTrace(thread);
  auto stacktrace_len = stacktrace->size();

  std::vector<std::set<uint32_t>> result;

  for (size_t idx = 0; idx < stacktrace_len; idx++) {
    auto element = stacktrace->getElement(idx);
    auto classname = jlStackTraceElement_getClassName(element)->toStdString();
    auto methodname = jlStackTraceElement_getMethodName(element)->toStdString();

    std::replace(classname.begin(), classname.end(), '.', '/');

    auto cls = fbjni::findClassLocal(classname.c_str());
    auto cls_methods = jlClass_getDeclaredMethods(cls);
    auto cls_methods_size = cls_methods->size();

    std::multimap<std::string, uint32_t> methods_map{};
    for (size_t meth_idx = 0; meth_idx < cls_methods_size; meth_idx++) {
      auto method = cls_methods->getElement(meth_idx);
#if defined(MUSEUM_VERSION_5_1_1)
      auto artMethod = jlrMethod_getArtMethod(method);
#endif
      methods_map.insert(
          std::make_pair(
            jlrMethod_getName(method)->toStdString(),
#if defined(MUSEUM_VERSION_5_1_1)
            (uint32_t) artMethod->getFieldValue(jlrArtMethod_dexMethodIndex)
#elif defined(MUSEUM_VERSION_6_0_1) || defined(MUSEUM_VERSION_7_0_0)
            (uint32_t) method->getFieldValue(jlrMethod_dexMethodIndex)
#endif
       ));
    }

    auto matches = methods_map.equal_range(methodname);
    std::set<uint32_t> overloads{};
    std::transform(
        matches.first,
        matches.second,
        std::inserter(overloads, overloads.begin()),
        [](const std::pair<std::string, uint32_t>& pair) {

        FBLOGV("Java Stack: %s -> %u", pair.first.c_str(), pair.second);
        return pair.second; }
    );
    result.emplace_back(overloads);
  }

  return result;
}

size_t getCppStackTrace(
  JNIEnv* env,
  void* nativeThread,
  std::array<uint32_t, kStackSize>& result) {

  InstallRuntime(env, nativeThread);

  JavaFrame frames[kStackSize]{};
  auto size = GetStackTrace(frames, kStackSize, nativeThread);

  for (size_t idx = 0; idx < size; idx++) {
    result[idx] = frames[idx].methodIdx;
  }

  return size;
}

bool compareStackTraces(
    std::array<uint32_t, kStackSize> cppStack,
    size_t cppStackSize,
    std::vector<std::set<uint32_t>> javaStack) {

  size_t javaStackSize = javaStack.size();

  // We expect the C++ stack trace to be a subset of the Java one (which has to
  // also call getStackTrace).
  if (cppStackSize > javaStackSize || cppStackSize == 0) {
    return false;
  }

  // The Java entries are a set of IDs because the Java mechanism does not help
  // us with overload resolution, so we have the IDs of all overloads.
  for (size_t i = 0; i < cppStackSize; i++) {
    auto cpp_frame = cppStack[cppStackSize - i - 1];
    auto java_options = javaStack[javaStackSize - i - 1];

#ifdef DEBUG
    std::stringstream options_str{};
    options_str << '[' << std::hex;
    for (auto& option: java_options) {
      options_str << option << " ";
    }
    options_str << ']';

    FBLOGV("Trying to find %x in any of %s", cpp_frame, options_str.str().c_str());
#endif

    if (java_options.find(cpp_frame) == java_options.end()) {
      return false;
    }
  }

  return true;
}

//
// We collect two stack traces, from the same JNI function (and therefore VM
// frame) - one from Java, using normal VM APIs and one using our
// async-safe C++ stack unwinder.
//
// The Java part effectively calls Thread.currentThread().getStackTrace() and
// then proceeds to look up the java.lang.reflect.Method object for each
// element and get its method index (stored in a private field by the runtime).
//
// Due to overloads and imprecise data, there is sometimes more than one method
// index per Java stack trace element.
//
// Once we have that, we execute the C++ counterpart safely in a ForkJail. The
// jail compares the C++ result against the Java source of truth and if they
// match, we exit with kExitCodeSuccess. Otherwise, we exit with a different
// status code.
//
jboolean check(JNIEnv* env, jclass) {

    constexpr int kExitCodeSuccess = 123;
    constexpr int kExitCodeFailure = 124;
    constexpr int kTimeoutSec = 1;

    auto jlThread_class = getThreadClass();
    auto jlThread_currentThread =
      jlThread_class->getStaticMethod<jobject()>("currentThread", "()Ljava/lang/Thread;");
    auto jlThread = jlThread_currentThread(jlThread_class);

    //
    // We must collect the Java stack trace before we fork because of internal
    // allocation locks within art.
    //
    std::vector<std::set<uint32_t>> javaStack;
    try {
      javaStack = getJavaStackTrace(jlThread);
    } catch(...) {
      return false;
    }

    auto jlThread_nativePeer = jlThread_class->getField<jlong>("nativePeer");
    auto nativePeer = jlThread->getFieldValue(jlThread_nativePeer);
    void* nativeThread = reinterpret_cast<void*>(nativePeer);

#if defined(MUSEUM_VERSION_6_0_1) || defined(MUSEUM_VERSION_7_0_0)
    // Initialize the runtime from the appropriate implementation for particular
    // version of ART subset.
    // As the Runtime is resolved from "libart", it's unsafe to be done in fork
    // jail.
    InitRuntime();
#endif

    forkjail::ForkJail jail([
      &javaStack,
      env,
      nativeThread] {

      try {
        std::array<uint32_t, kStackSize> cppStack;
        auto cppStackSize = getCppStackTrace(env, nativeThread, cppStack);

        if (compareStackTraces(cppStack, cppStackSize, javaStack)) {
          FBLOGV("compareStackTraces returned true");
          _exit(kExitCodeSuccess);
        }

        FBLOGV("compareStackTraces returned false");
      } catch(...) {
        FBLOGV("Ignored exception");
        // intentionally ignored
      }

      _exit(kExitCodeFailure);
    }, kTimeoutSec);

    try {
      auto child = jail.forkAndRun();
      // Child process would never reach here. Only the parent continues.
      // Wait for the child to exit.
      int status = 0;

      do {
        if (waitpid(child, &status, 0) != child) {
          throw std::system_error(errno, std::system_category(), "waitpid");
        }
      } while(!WIFEXITED(status) && !WIFSIGNALED(status));

      FBLOGD("Cpp stack child exited: %i status: %i signalled: %i signal: %i",
          WIFEXITED(status), WEXITSTATUS(status),
          WIFSIGNALED(status), WTERMSIG(status));

      if (!WIFEXITED(status) || WEXITSTATUS(status) != kExitCodeSuccess) {
        return false;
      }

      return true;
    } catch (std::system_error& ex) {
      FBLOGE("Caught system error: %s", ex.what());
      return false;
    }
}

} // namespace

#if defined(MUSEUM_VERSION_5_1_1)
void registerNatives_5_1_1() {
#elif defined(MUSEUM_VERSION_6_0_1)
void registerNatives_6_0_1() {
#elif defined(MUSEUM_VERSION_7_0_0)
void registerNatives_7_0_0() {
#endif
  constexpr auto kArtCompatibilityType = "com/facebook/profilo/provider/stacktrace/ArtCompatibility";

  fbjni::registerNatives(
    kArtCompatibilityType,
    {
#if defined(MUSEUM_VERSION_5_1_1)
      makeNativeMethod("nativeCheckArt5_1", check),
#elif defined(MUSEUM_VERSION_6_0_1)
      makeNativeMethod("nativeCheckArt6_0", check),
#elif defined(MUSEUM_VERSION_7_0_0)
      makeNativeMethod("nativeCheckArt7_0", check),
#endif
    });
}

} // artcompat
} // profilo
} // facebook
