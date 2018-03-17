/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_MTERP_MTERP_H_
#define ART_RUNTIME_INTERPRETER_MTERP_MTERP_H_

/*
 * Mterp assembly handler bases
 */
extern "C" void* artMterpAsmInstructionStart[];
extern "C" void* artMterpAsmInstructionEnd[];
extern "C" void* artMterpAsmAltInstructionStart[];
extern "C" void* artMterpAsmAltInstructionEnd[];

namespace art {
namespace interpreter {

void InitMterpTls(Thread* self);
void CheckMterpAsmConstants();
extern "C" bool MterpShouldSwitchInterpreters();

}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_MTERP_MTERP_H_
