/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_BASE_LOGGING_H_
#define ART_RUNTIME_BASE_LOGGING_H_

#include <ostream>
#include <sstream>

#include "android-base/logging.h"
#include "base/macros.h"

namespace art {

// Make libbase's LogSeverity more easily available.
using ::android::base::LogSeverity;
using ::android::base::ScopedLogSeverity;

// Abort function.
using AbortFunction = void(const char*);

// The members of this struct are the valid arguments to VLOG and VLOG_IS_ON in code,
// and the "-verbose:" command line argument.
struct LogVerbosity {
  bool class_linker;  // Enabled with "-verbose:class".
  bool collector;
  bool compiler;
  bool deopt;
  bool gc;
  bool heap;
  bool jdwp;
  bool jit;
  bool jni;
  bool monitor;
  bool oat;
  bool profiler;
  bool signals;
  bool simulator;
  bool startup;
  bool third_party_jni;  // Enabled with "-verbose:third-party-jni".
  bool threads;
  bool verifier;
  bool image;
  bool systrace_lock_logging;  // Enabled with "-verbose:sys-locks".
  bool agents;
  bool dex;  // Some dex access output etc.
};

// Global log verbosity setting, initialized by InitLogging.
extern LogVerbosity gLogVerbosity;

// Runtime debug flags are flags that have a runtime component, that is, their value can be changed.
// This is meant to implement fast vs slow debug builds, in that certain debug flags can be turned
// on and off. To that effect, expose two macros to help implement and globally drive these flags:
//
// In the header, declare a (class) flag like this:
//
//   class C {
//     DECLARE_RUNTIME_DEBUG_FLAG(kFlag);
//   };
//
// This will declare a flag kFlag that is a constexpr false in release builds, and a static field
// in debug builds. Usage is than uniform as C::kFlag.
//
// In the cc file, define the flag like this:
//
//   DEFINE_RUNTIME_DEBUG_FLAG(C, kFlag);
//
// This will define the static storage, as necessary, and register the flag with the runtime
// infrastructure to toggle the value.

#ifdef NDEBUG
#define DECLARE_RUNTIME_DEBUG_FLAG(x) \
  static constexpr bool x = false;
// Note: the static_assert in the following only works for public flags. Fix this when we cross
//       the line at some point.
#define DEFINE_RUNTIME_DEBUG_FLAG(C, x) \
  static_assert(!C::x, "Unexpected enabled flag in release build");
#else
#define DECLARE_RUNTIME_DEBUG_FLAG(x) \
  static bool x;
#define DEFINE_RUNTIME_DEBUG_FLAG(C, x) \
  bool C::x = RegisterRuntimeDebugFlag(&C::x);
#endif  // NDEBUG

bool RegisterRuntimeDebugFlag(bool* runtime_debug_flag);
void SetRuntimeDebugFlagsEnabled(bool enabled);

// 0 if not abort, non-zero if an abort is in progress. Used on fatal exit to prevents recursive
// aborts. Global declaration allows us to disable some error checking to ensure fatal shutdown
// makes forward progress.
extern std::atomic<unsigned int> gAborting;

// Configure logging based on ANDROID_LOG_TAGS environment variable.
// We need to parse a string that looks like
//
//      *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
//
// The tag (or '*' for the global level) comes first, followed by a colon
// and a letter indicating the minimum priority level we're expected to log.
// This can be used to reveal or conceal logs with specific tags.
extern void InitLogging(char* argv[], AbortFunction& default_aborter);

// Returns the command line used to invoke the current tool or null if InitLogging hasn't been
// performed.
extern const char* GetCmdLine();

// The command used to start the ART runtime, such as "/system/bin/dalvikvm". If InitLogging hasn't
// been performed then just returns "art"
extern const char* ProgramInvocationName();

// A short version of the command used to start the ART runtime, such as "dalvikvm". If InitLogging
// hasn't been performed then just returns "art"
extern const char* ProgramInvocationShortName();

class LogHelper {
 public:
  // A logging helper for logging a single line. Can be used with little stack.
  static void LogLineLowStack(const char* file,
                              unsigned int line,
                              android::base::LogSeverity severity,
                              const char* msg);

 private:
  DISALLOW_ALLOCATION();
  DISALLOW_COPY_AND_ASSIGN(LogHelper);
};

// Is verbose logging enabled for the given module? Where the module is defined in LogVerbosity.
#define VLOG_IS_ON(module) UNLIKELY(::art::gLogVerbosity.module)

// Variant of LOG that logs when verbose logging is enabled for a module. For example,
// VLOG(jni) << "A JNI operation was performed";
#define VLOG(module) if (VLOG_IS_ON(module)) LOG(INFO)

// Return the stream associated with logging for the given module.
#define VLOG_STREAM(module) LOG_STREAM(INFO)

}  // namespace art

#endif  // ART_RUNTIME_BASE_LOGGING_H_
