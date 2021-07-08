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

#include "ArtCompatibilityRunner.h"

#include <fb/log.h>
#include <fbjni/fbjni.h>
#include <profilo/profiler/JavaBaseTracer.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <ios>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <forkjail/ForkJail.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace artcompat {

struct JavaFrame {
  std::string class_descriptor{""};
  std::string name{""};
  std::set<int64_t> identifiers;
};

struct CppUnwinderJavaFrame {
  const char* class_descriptor;
  const char* name;
  int32_t identifier;
};

namespace {

static const int kStackSize = 128;

fbjni::local_ref<jclass> getThreadClass() {
  return fbjni::findClassLocal("java/lang/Thread");
}

uint32_t getDexMethodIndex5(fbjni::alias_ref<jobject> method) {
  auto jlrMethod_class = fbjni::findClassLocal("java/lang/reflect/Method");
  auto jlrArtMethod_class =
      fbjni::findClassLocal("java/lang/reflect/ArtMethod");
  auto jlrMethod_getArtMethod = jlrMethod_class->getMethod<jobject()>(
      "getArtMethod", "()Ljava/lang/reflect/ArtMethod;");
  auto jlrArtMethod_dexMethodIndex =
      jlrArtMethod_class->getField<jint>("dexMethodIndex");
  auto artMethod = jlrMethod_getArtMethod(method);
  return (uint32_t)artMethod->getFieldValue(jlrArtMethod_dexMethodIndex);
}

uint32_t getDexMethodIndex678(fbjni::alias_ref<jobject> method) {
  auto jlrMethod_class = fbjni::findClassLocal("java/lang/reflect/Method");
  auto jlrMethod_dexMethodIndex =
      jlrMethod_class->getField<jint>("dexMethodIndex");
  return (uint32_t)method->getFieldValue(jlrMethod_dexMethodIndex);
}

std::vector<JavaFrame> getJavaStackTrace(
    versions::AndroidVersion version,
    fbjni::alias_ref<jobject> thread) {
  auto jlThread_class = getThreadClass();
  auto jlThread_getStackTrace =
      jlThread_class->getMethod<fbjni::jtypeArray<jobject>()>(
          "getStackTrace", "()[Ljava/lang/StackTraceElement;");

  auto jlStackTraceElement_class =
      fbjni::findClassLocal("java/lang/StackTraceElement");
  auto jlStackTraceElement_getClassName =
      jlStackTraceElement_class->getMethod<jstring()>("getClassName");
  auto jlStackTraceElement_getMethodName =
      jlStackTraceElement_class->getMethod<jstring()>("getMethodName");

  auto jlClass_class = fbjni::findClassLocal("java/lang/Class");
  auto jlClass_getDeclaredMethods =
      jlClass_class->getMethod<fbjni::jtypeArray<jobject>()>(
          "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");

  auto jlrMethod_class = fbjni::findClassLocal("java/lang/reflect/Method");
  auto jlrMethod_getName = jlrMethod_class->getMethod<jstring()>("getName");

  auto stacktrace = jlThread_getStackTrace(thread);
  auto stacktrace_len = stacktrace->size();

  std::vector<JavaFrame> result;

  for (size_t idx = 0; idx < stacktrace_len; idx++) {
    auto element = stacktrace->getElement(idx);
    auto classname = jlStackTraceElement_getClassName(element)->toStdString();
    auto methodname = jlStackTraceElement_getMethodName(element)->toStdString();

    std::replace(classname.begin(), classname.end(), '.', '/');

    JavaFrame frame{};
    frame.class_descriptor = "L" + classname + ";";
    frame.name = methodname;

    // On pre-Pie Android we can also verify the method idx by
    // extracting all possible overloads for the method in the
    // stack trace and using reflection to collect their method idxs.
    if (version < versions::ANDROID_9_0) {
      auto cls = fbjni::findClassLocal(classname.c_str());
      auto cls_methods = jlClass_getDeclaredMethods(cls);
      auto cls_methods_size = cls_methods->size();

      std::multimap<std::string, uint32_t> methods_map{};
      for (size_t meth_idx = 0; meth_idx < cls_methods_size; meth_idx++) {
        auto method = cls_methods->getElement(meth_idx);
        uint32_t dexMethodIndex;
        if (version == versions::ANDROID_5) {
          dexMethodIndex = getDexMethodIndex5(method);
        } else {
          dexMethodIndex = getDexMethodIndex678(method);
        }
        methods_map.insert(std::make_pair(
            jlrMethod_getName(method)->toStdString(), dexMethodIndex));
      }

      auto matches = methods_map.equal_range(methodname);
      std::set<int64_t> overloads{};
      std::transform(
          matches.first,
          matches.second,
          std::inserter(overloads, overloads.begin()),
          [](const std::pair<std::string, uint32_t>& pair) {
            return pair.second;
          });

      frame.identifiers = std::move(overloads);
    }

    result.emplace_back(std::move(frame));
  }

  return result;
}

size_t getCppStackTrace(
    profiler::JavaBaseTracer* tracer,
    std::array<CppUnwinderJavaFrame, kStackSize>& result) {
  uint16_t depth = 0;

  int64_t ints[kStackSize];
  char const* method_names[kStackSize];
  char const* class_descriptors[kStackSize];
  auto ret = tracer->collectJavaStack(
      nullptr, ints, method_names, class_descriptors, depth, kStackSize);
  for (size_t idx = 0; idx < depth; idx++) {
    result[idx] = CppUnwinderJavaFrame{
        .class_descriptor = class_descriptors[idx],
        .name = method_names[idx],
        .identifier = static_cast<int32_t>(ints[idx] >> 32),
    };
  }

  if (ret != StackCollectionRetcode::SUCCESS) {
    return 0;
  }

  return depth;
}

bool compareStackTraces(
    std::array<CppUnwinderJavaFrame, kStackSize>& cppStack,
    size_t cppStackSize,
    std::vector<JavaFrame>& javaStack) {
  size_t javaStackSize = javaStack.size();

  // We expect the C++ stack trace to be a subset of the Java one (which has to
  // also call getStackTrace).
  if (cppStackSize > javaStackSize || cppStackSize == 0) {
    return false;
  }

  // We may get different types of data from the Java and C++ sides.
  // In particular, we may not have method idxs on the Java side or
  // we may not have class descriptors and method names on the C++ side.
  //
  // Attempt to match what we have and complain if there's no
  // intersection of available information.
  for (size_t i = 0; i < cppStackSize; i++) {
    auto& cpp_frame = cppStack[cppStackSize - i - 1];
    auto& java_frame = javaStack[javaStackSize - i - 1];

    // If either of identifiers is not available the check will be limited to
    // descriptors only.
    if (java_frame.identifiers.size() != 0 && cpp_frame.identifier != 0) {
      int64_t cpp_identifier = cpp_frame.identifier;

      // Ensure that the C++ identifier is in the list of
      // possibilities from Java.
      if (java_frame.identifiers.find(cpp_identifier) ==
          java_frame.identifiers.end()) {
        FBLOGW("C++ identifier not in Java list");
        return false;
      }
    }

    if (cpp_frame.class_descriptor == nullptr || cpp_frame.name == nullptr) {
      FBLOGW("Cpp Unwind returned empty class or method symbol(s)");
      return false;
    }

    // We want Class + Name to be extra sure.
    if (java_frame.class_descriptor.compare(cpp_frame.class_descriptor) != 0) {
      FBLOGW(
          "Class descriptors did not match Java:%s C++:%s",
          java_frame.class_descriptor.c_str(),
          cpp_frame.class_descriptor);
      return false;
    }

    if (java_frame.name.compare(cpp_frame.name) != 0) {
      FBLOGW(
          "Method names did not match Java:%s C++:%s",
          java_frame.name.c_str(),
          cpp_frame.name);
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
// element and get its method index (stored in a private field by the
// runtime).
//
// Due to overloads and imprecise data, there is sometimes more than one
// method index per Java stack trace element.
//
// Once we have that, we execute the C++ counterpart safely in a ForkJail. The
// jail compares the C++ result against the Java source of truth and if they
// match, we exit with kExitCodeSuccess. Otherwise, we exit with a different
// status code.
//
bool runJavaCompatibilityCheckInternal(
    versions::AndroidVersion version,
    profiler::JavaBaseTracer* tracer) {
  constexpr int kExitCodeSuccess = 100;
  constexpr int kExitCodeFailure = 150;
  constexpr int kTimeoutSec = 1;

  auto jlThread_class = getThreadClass();
  auto jlThread_currentThread = jlThread_class->getStaticMethod<jobject()>(
      "currentThread", "()Ljava/lang/Thread;");
  auto jlThread = jlThread_currentThread(jlThread_class);

  //
  // We must collect the Java stack trace before we fork because of internal
  // allocation locks within art.
  //
  std::vector<JavaFrame> javaStack;
  try {
    javaStack = getJavaStackTrace(version, jlThread);
  } catch (...) {
    return false;
  }

  // Performs initialization in the parent, before we fork.
  tracer->prepare();

  forkjail::ForkJail jail(
      [&javaStack, tracer] {
        try {
          tracer->startTracing();

          std::array<CppUnwinderJavaFrame, kStackSize> cppStack;
          auto cppStackSize = getCppStackTrace(tracer, cppStack);

          if (compareStackTraces(cppStack, cppStackSize, javaStack)) {
            FBLOGV("compareStackTraces returned true");
            forkjail::ForkJail::real_exit(kExitCodeSuccess);
          }

          FBLOGV("compareStackTraces returned false");
        } catch (...) {
          FBLOGV("Ignored exception");
          // intentionally ignored
        }

        forkjail::ForkJail::real_exit(kExitCodeFailure);
      },
      kTimeoutSec);

  try {
    auto child = jail.forkAndRun();
    // Child process would never reach here. Only the parent continues.
    // Wait for the child to exit.
    int status = 0;

    do {
      if (waitpid(child, &status, 0) != child) {
        throw std::system_error(errno, std::system_category(), "waitpid");
      }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    FBLOGD(
        "Cpp stack child exited: %i status: %i (%s) signalled: %i signal: %i",
        WIFEXITED(status),
        WEXITSTATUS(status),
        WEXITSTATUS(status) == kExitCodeSuccess ? "success" : "failure",
        WIFSIGNALED(status),
        WTERMSIG(status));

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

bool runJavaCompatibilityCheck(
    versions::AndroidVersion version,
    profiler::JavaBaseTracer* tracer) {
  return runJavaCompatibilityCheckInternal(version, tracer);
}

} // namespace artcompat
} // namespace profilo
} // namespace facebook
