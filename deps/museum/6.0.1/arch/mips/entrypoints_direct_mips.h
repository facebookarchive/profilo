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

#ifndef ART_RUNTIME_ARCH_MIPS_ENTRYPOINTS_DIRECT_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_ENTRYPOINTS_DIRECT_MIPS_H_

#include "entrypoints/quick/quick_entrypoints_enum.h"

namespace art {

/* Returns true if entrypoint contains direct reference to
   native implementation. The list is required as direct
   entrypoints need additional handling during invocation.*/
static constexpr bool IsDirectEntrypoint(QuickEntrypointEnum entrypoint) {
  return
      entrypoint == kQuickInstanceofNonTrivial ||
      entrypoint == kQuickA64Load ||
      entrypoint == kQuickA64Store ||
      entrypoint == kQuickFmod ||
      entrypoint == kQuickFmodf ||
      entrypoint == kQuickMemcpy ||
      entrypoint == kQuickL2d ||
      entrypoint == kQuickL2f ||
      entrypoint == kQuickD2iz ||
      entrypoint == kQuickF2iz ||
      entrypoint == kQuickD2l ||
      entrypoint == kQuickF2l ||
      entrypoint == kQuickLdiv ||
      entrypoint == kQuickLmod ||
      entrypoint == kQuickLmul ||
      entrypoint == kQuickCmpgDouble ||
      entrypoint == kQuickCmpgFloat ||
      entrypoint == kQuickCmplDouble ||
      entrypoint == kQuickCmplFloat;
}

}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS_ENTRYPOINTS_DIRECT_MIPS_H_
