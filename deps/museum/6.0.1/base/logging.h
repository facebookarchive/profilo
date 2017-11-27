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

#include <memory>
#include <ostream>

#include "base/macros.h"

namespace art {

enum LogSeverity {
  VERBOSE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL,
  INTERNAL_FATAL,  // For Runtime::Abort.
};

// The members of this struct are the valid arguments to VLOG and VLOG_IS_ON in code,
// and the "-verbose:" command line argument.
struct LogVerbosity {
  bool class_linker;  // Enabled with "-verbose:class".
  bool compiler;
  bool gc;
  bool heap;
  bool jdwp;
  bool jit;
  bool jni;
  bool monitor;
  bool oat;
  bool profiler;
  bool signals;
  bool startup;
  bool third_party_jni;  // Enabled with "-verbose:third-party-jni".
  bool threads;
  bool verifier;
};

// Global log verbosity setting, initialized by InitLogging.
extern LogVerbosity gLogVerbosity;

// 0 if not abort, non-zero if an abort is in progress. Used on fatal exit to prevents recursive
// aborts. Global declaration allows us to disable some error checking to ensure fatal shutdown
// makes forward progress.
extern unsigned int gAborting;

// Configure logging based on ANDROID_LOG_TAGS environment variable.
// We need to parse a string that looks like
//
//      *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
//
// The tag (or '*' for the global level) comes first, followed by a colon
// and a letter indicating the minimum priority level we're expected to log.
// This can be used to reveal or conceal logs with specific tags.
extern void InitLogging(char* argv[]);

// Returns the command line used to invoke the current tool or null if InitLogging hasn't been
// performed.
extern const char* GetCmdLine();

// The command used to start the ART runtime, such as "/system/bin/dalvikvm". If InitLogging hasn't
// been performed then just returns "art"
extern const char* ProgramInvocationName();

// A short version of the command used to start the ART runtime, such as "dalvikvm". If InitLogging
// hasn't been performed then just returns "art"
extern const char* ProgramInvocationShortName();

// Logs a message to logcat on Android otherwise to stderr. If the severity is FATAL it also causes
// an abort. For example: LOG(FATAL) << "We didn't expect to reach here";
#define LOG(severity) ::art::LogMessage(__FILE__, __LINE__, severity, -1).stream()

// A variant of LOG that also logs the current errno value. To be used when library calls fail.
#define PLOG(severity) ::art::LogMessage(__FILE__, __LINE__, severity, errno).stream()

// Marker that code is yet to be implemented.
#define UNIMPLEMENTED(level) LOG(level) << __PRETTY_FUNCTION__ << " unimplemented "

// Is verbose logging enabled for the given module? Where the module is defined in LogVerbosity.
#define VLOG_IS_ON(module) UNLIKELY(::art::gLogVerbosity.module)

// Variant of LOG that logs when verbose logging is enabled for a module. For example,
// VLOG(jni) << "A JNI operation was performed";
#define VLOG(module) \
  if (VLOG_IS_ON(module)) \
    ::art::LogMessage(__FILE__, __LINE__, INFO, -1).stream()

// Return the stream associated with logging for the given module.
#define VLOG_STREAM(module) ::art::LogMessage(__FILE__, __LINE__, INFO, -1).stream()

// Check whether condition x holds and LOG(FATAL) if not. The value of the expression x is only
// evaluated once. Extra logging can be appended using << after. For example,
// CHECK(false == true) results in a log message of "Check failed: false == true".
#define CHECK(x) \
  if (UNLIKELY(!(x))) \
    ::art::LogMessage(__FILE__, __LINE__, ::art::FATAL, -1).stream() \
        << "Check failed: " #x << " "

// Helper for CHECK_xx(x,y) macros.
#define CHECK_OP(LHS, RHS, OP) \
  for (auto _values = ::art::MakeEagerEvaluator(LHS, RHS); \
       UNLIKELY(!(_values.lhs OP _values.rhs)); /* empty */) \
    ::art::LogMessage(__FILE__, __LINE__, ::art::FATAL, -1).stream() \
        << "Check failed: " << #LHS << " " << #OP << " " << #RHS \
        << " (" #LHS "=" << _values.lhs << ", " #RHS "=" << _values.rhs << ") "


// Check whether a condition holds between x and y, LOG(FATAL) if not. The value of the expressions
// x and y is evaluated once. Extra logging can be appended using << after. For example,
// CHECK_NE(0 == 1, false) results in "Check failed: false != false (0==1=false, false=false) ".
#define CHECK_EQ(x, y) CHECK_OP(x, y, ==)
#define CHECK_NE(x, y) CHECK_OP(x, y, !=)
#define CHECK_LE(x, y) CHECK_OP(x, y, <=)
#define CHECK_LT(x, y) CHECK_OP(x, y, <)
#define CHECK_GE(x, y) CHECK_OP(x, y, >=)
#define CHECK_GT(x, y) CHECK_OP(x, y, >)

// Helper for CHECK_STRxx(s1,s2) macros.
#define CHECK_STROP(s1, s2, sense) \
  if (UNLIKELY((strcmp(s1, s2) == 0) != sense)) \
    LOG(::art::FATAL) << "Check failed: " \
        << "\"" << s1 << "\"" \
        << (sense ? " == " : " != ") \
        << "\"" << s2 << "\""

// Check for string (const char*) equality between s1 and s2, LOG(FATAL) if not.
#define CHECK_STREQ(s1, s2) CHECK_STROP(s1, s2, true)
#define CHECK_STRNE(s1, s2) CHECK_STROP(s1, s2, false)

// Perform the pthread function call(args), LOG(FATAL) on error.
#define CHECK_PTHREAD_CALL(call, args, what) \
  do { \
    int rc = call args; \
    if (rc != 0) { \
      errno = rc; \
      PLOG(::art::FATAL) << # call << " failed for " << what; \
    } \
  } while (false)

// CHECK that can be used in a constexpr function. For example,
//    constexpr int half(int n) {
//      return
//          DCHECK_CONSTEXPR(n >= 0, , 0)
//          CHECK_CONSTEXPR((n & 1) == 0), << "Extra debugging output: n = " << n, 0)
//          n / 2;
//    }
#define CHECK_CONSTEXPR(x, out, dummy) \
  (UNLIKELY(!(x))) ? (LOG(::art::FATAL) << "Check failed: " << #x out, dummy) :


// DCHECKs are debug variants of CHECKs only enabled in debug builds. Generally CHECK should be
// used unless profiling identifies a CHECK as being in performance critical code.
#if defined(NDEBUG)
static constexpr bool kEnableDChecks = false;
#else
static constexpr bool kEnableDChecks = true;
#endif

#define DCHECK(x) if (::art::kEnableDChecks) CHECK(x)
#define DCHECK_EQ(x, y) if (::art::kEnableDChecks) CHECK_EQ(x, y)
#define DCHECK_NE(x, y) if (::art::kEnableDChecks) CHECK_NE(x, y)
#define DCHECK_LE(x, y) if (::art::kEnableDChecks) CHECK_LE(x, y)
#define DCHECK_LT(x, y) if (::art::kEnableDChecks) CHECK_LT(x, y)
#define DCHECK_GE(x, y) if (::art::kEnableDChecks) CHECK_GE(x, y)
#define DCHECK_GT(x, y) if (::art::kEnableDChecks) CHECK_GT(x, y)
#define DCHECK_STREQ(s1, s2) if (::art::kEnableDChecks) CHECK_STREQ(s1, s2)
#define DCHECK_STRNE(s1, s2) if (::art::kEnableDChecks) CHECK_STRNE(s1, s2)
#if defined(NDEBUG)
#define DCHECK_CONSTEXPR(x, out, dummy)
#else
#define DCHECK_CONSTEXPR(x, out, dummy) CHECK_CONSTEXPR(x, out, dummy)
#endif

// Temporary class created to evaluate the LHS and RHS, used with MakeEagerEvaluator to infer the
// types of LHS and RHS.
template <typename LHS, typename RHS>
struct EagerEvaluator {
  EagerEvaluator(LHS l, RHS r) : lhs(l), rhs(r) { }
  LHS lhs;
  RHS rhs;
};

// Helper function for CHECK_xx.
template <typename LHS, typename RHS>
static inline EagerEvaluator<LHS, RHS> MakeEagerEvaluator(LHS lhs, RHS rhs) {
  return EagerEvaluator<LHS, RHS>(lhs, rhs);
}

// Explicitly instantiate EagerEvalue for pointers so that char*s aren't treated as strings. To
// compare strings use CHECK_STREQ and CHECK_STRNE. We rely on signed/unsigned warnings to
// protect you against combinations not explicitly listed below.
#define EAGER_PTR_EVALUATOR(T1, T2) \
  template <> struct EagerEvaluator<T1, T2> { \
    EagerEvaluator(T1 l, T2 r) \
        : lhs(reinterpret_cast<const void*>(l)), \
          rhs(reinterpret_cast<const void*>(r)) { } \
    const void* lhs; \
    const void* rhs; \
  }
EAGER_PTR_EVALUATOR(const char*, const char*);
EAGER_PTR_EVALUATOR(const char*, char*);
EAGER_PTR_EVALUATOR(char*, const char*);
EAGER_PTR_EVALUATOR(char*, char*);
EAGER_PTR_EVALUATOR(const unsigned char*, const unsigned char*);
EAGER_PTR_EVALUATOR(const unsigned char*, unsigned char*);
EAGER_PTR_EVALUATOR(unsigned char*, const unsigned char*);
EAGER_PTR_EVALUATOR(unsigned char*, unsigned char*);
EAGER_PTR_EVALUATOR(const signed char*, const signed char*);
EAGER_PTR_EVALUATOR(const signed char*, signed char*);
EAGER_PTR_EVALUATOR(signed char*, const signed char*);
EAGER_PTR_EVALUATOR(signed char*, signed char*);

// Data for the log message, not stored in LogMessage to avoid increasing the stack size.
class LogMessageData;

// A LogMessage is a temporarily scoped object used by LOG and the unlikely part of a CHECK. The
// destructor will abort if the severity is FATAL.
class LogMessage {
 public:
  LogMessage(const char* file, unsigned int line, LogSeverity severity, int error);

  ~LogMessage();  // TODO: enable LOCKS_EXCLUDED(Locks::logging_lock_).

  // Returns the stream associated with the message, the LogMessage performs output when it goes
  // out of scope.
  std::ostream& stream();

  // The routine that performs the actual logging.
  static void LogLine(const char* file, unsigned int line, LogSeverity severity, const char* msg);

  // A variant of the above for use with little stack.
  static void LogLineLowStack(const char* file, unsigned int line, LogSeverity severity,
                              const char* msg);

 private:
  const std::unique_ptr<LogMessageData> data_;

  DISALLOW_COPY_AND_ASSIGN(LogMessage);
};

// Allows to temporarily change the minimum severity level for logging.
class ScopedLogSeverity {
 public:
  explicit ScopedLogSeverity(LogSeverity level);
  ~ScopedLogSeverity();

 private:
  LogSeverity old_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_LOGGING_H_
