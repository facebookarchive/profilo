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

#ifndef ART_RUNTIME_INTERPRETER_INTERPRETER_INTRINSICS_H_
#define ART_RUNTIME_INTERPRETER_INTERPRETER_INTRINSICS_H_

#include "jvalue.h"

namespace art {

class ArtMethod;
class Instruction;
class ShadowFrame;

namespace interpreter {

// Invokes to methods identified as intrinics are routed here.  If there is
// no interpreter implementation, return false and a normal invoke will proceed.
bool MterpHandleIntrinsic(ShadowFrame* shadow_frame,
                          ArtMethod* const called_method,
                          const Instruction* inst,
                          uint16_t inst_data,
                          JValue* result_register);

}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_INTRINSICS_H_
