/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_CALLEE_SAVE_TYPE_H_
#define ART_RUNTIME_BASE_CALLEE_SAVE_TYPE_H_

#include <cstddef>
#include <ostream>

namespace art {

// Returns a special method that describes all callee saves being spilled to the stack.
enum class CalleeSaveType : uint32_t {
  kSaveAllCalleeSaves,  // All callee-save registers.
  kSaveRefsOnly,        // Only those callee-save registers that can hold references.
  kSaveRefsAndArgs,     // References (see above) and arguments (usually caller-save registers).
  kSaveEverything,      // All registers, including both callee-save and caller-save.
  kLastCalleeSaveType   // Value used for iteration.
};
std::ostream& operator<<(std::ostream& os, const CalleeSaveType& rhs);

}  // namespace art

#endif  // ART_RUNTIME_BASE_CALLEE_SAVE_TYPE_H_
