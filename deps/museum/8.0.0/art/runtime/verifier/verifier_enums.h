/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_VERIFIER_ENUMS_H_
#define ART_RUNTIME_VERIFIER_VERIFIER_ENUMS_H_

#include <stdint.h>

namespace art {
namespace verifier {

// The mode that the verifier should run as.
enum class VerifyMode : int8_t {
  kNone,      // Everything is assumed verified.
  kEnable,    // Standard verification, try pre-verifying at compile-time.
  kSoftFail,  // Force a soft fail, punting to the interpreter with access checks.
};

// The outcome of verification.
enum class FailureKind {
  kNoFailure,
  kSoftFailure,
  kHardFailure,
};
std::ostream& operator<<(std::ostream& os, const FailureKind& rhs);

// How to log hard failures during verification.
enum class HardFailLogMode {
  kLogNone,                               // Don't log hard failures at all.
  kLogVerbose,                            // Log with severity VERBOSE.
  kLogWarning,                            // Log with severity WARNING.
  kLogInternalFatal,                      // Log with severity FATAL_WITHOUT_ABORT
};

/*
 * "Direct" and "virtual" methods are stored independently. The type of call used to invoke the
 * method determines which list we search, and whether we travel up into superclasses.
 *
 * (<clinit>, <init>, and methods declared "private" or "static" are stored in the "direct" list.
 * All others are stored in the "virtual" list.)
 */
enum MethodType {
  METHOD_UNKNOWN  = 0,
  METHOD_DIRECT,      // <init>, private
  METHOD_STATIC,      // static
  METHOD_VIRTUAL,     // virtual
  METHOD_SUPER,       // super
  METHOD_INTERFACE,   // interface
  METHOD_POLYMORPHIC  // polymorphic
};
std::ostream& operator<<(std::ostream& os, const MethodType& rhs);

/*
 * An enumeration of problems that can turn up during verification.
 * Both VERIFY_ERROR_BAD_CLASS_SOFT and VERIFY_ERROR_BAD_CLASS_HARD denote failures that cause
 * the entire class to be rejected. However, VERIFY_ERROR_BAD_CLASS_SOFT denotes a soft failure
 * that can potentially be corrected, and the verifier will try again at runtime.
 * VERIFY_ERROR_BAD_CLASS_HARD denotes a hard failure that can't be corrected, and will cause
 * the class to remain uncompiled. Other errors denote verification errors that cause bytecode
 * to be rewritten to fail at runtime.
 */
enum VerifyError {
  VERIFY_ERROR_BAD_CLASS_HARD = 1,        // VerifyError; hard error that skips compilation.
  VERIFY_ERROR_BAD_CLASS_SOFT = 2,        // VerifyError; soft error that verifies again at runtime.

  VERIFY_ERROR_NO_CLASS = 4,              // NoClassDefFoundError.
  VERIFY_ERROR_NO_FIELD = 8,              // NoSuchFieldError.
  VERIFY_ERROR_NO_METHOD = 16,            // NoSuchMethodError.
  VERIFY_ERROR_ACCESS_CLASS = 32,         // IllegalAccessError.
  VERIFY_ERROR_ACCESS_FIELD = 64,         // IllegalAccessError.
  VERIFY_ERROR_ACCESS_METHOD = 128,       // IllegalAccessError.
  VERIFY_ERROR_CLASS_CHANGE = 256,        // IncompatibleClassChangeError.
  VERIFY_ERROR_INSTANTIATION = 512,       // InstantiationError.
  // For opcodes that don't have complete verifier support,  we need a way to continue
  // execution at runtime without attempting to re-verify (since we know it will fail no
  // matter what). Instead, run as the interpreter in a special "do access checks" mode
  // which will perform verifier-like checking on the fly.
  VERIFY_ERROR_FORCE_INTERPRETER = 1024,  // Skip the verification phase at runtime;
                                          // force the interpreter to do access checks.
                                          // (sets a soft fail at compile time).
  VERIFY_ERROR_LOCKING = 2048,            // Could not guarantee balanced locking. This should be
                                          // punted to the interpreter with access checks.
};
std::ostream& operator<<(std::ostream& os, const VerifyError& rhs);

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_ENUMS_H_
