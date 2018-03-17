/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_REGISTER_LINE_INL_H_
#define ART_RUNTIME_VERIFIER_REGISTER_LINE_INL_H_

#include <museum/5.1.1/art/runtime/verifier/register_line.h>

#include <museum/5.1.1/art/runtime/verifier/method_verifier.h>
#include <museum/5.1.1/art/runtime/verifier/reg_type_cache-inl.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace verifier {

inline RegType& RegisterLine::GetRegisterType(uint32_t vsrc) const {
  // The register index was validated during the static pass, so we don't need to check it here.
  DCHECK_LT(vsrc, num_regs_);
  return verifier_->GetRegTypeCache()->GetFromId(line_[vsrc]);
}

}  // namespace verifier
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_VERIFIER_REGISTER_LINE_INL_H_
