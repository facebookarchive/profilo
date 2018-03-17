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

#ifndef ART_RUNTIME_VERIFIER_VERIFIER_COMPILER_BINDING_H_
#define ART_RUNTIME_VERIFIER_VERIFIER_COMPILER_BINDING_H_

#include <museum/8.1.0/external/libcxx/inttypes.h>

#include <museum/8.1.0/art/runtime/base/macros.h>
#include <museum/8.1.0/art/runtime/verifier/verifier_enums.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace verifier {

ALWAYS_INLINE
static inline bool CanCompilerHandleVerificationFailure(uint32_t encountered_failure_types) {
  constexpr uint32_t unresolved_mask = verifier::VerifyError::VERIFY_ERROR_NO_CLASS
      | verifier::VerifyError::VERIFY_ERROR_ACCESS_CLASS
      | verifier::VerifyError::VERIFY_ERROR_ACCESS_FIELD
      | verifier::VerifyError::VERIFY_ERROR_ACCESS_METHOD;
  return (encountered_failure_types & (~unresolved_mask)) == 0;
}

}  // namespace verifier
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_COMPILER_BINDING_H_
