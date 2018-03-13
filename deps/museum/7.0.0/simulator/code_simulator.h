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

#ifndef ART_RUNTIME_SIMULATOR_CODE_SIMULATOR_H_
#define ART_RUNTIME_SIMULATOR_CODE_SIMULATOR_H_

#include "arch/instruction_set.h"

namespace art {

class CodeSimulator {
 public:
  CodeSimulator() {}
  virtual ~CodeSimulator() {}
  // Returns a null pointer if a simulator cannot be found for target_isa.
  static CodeSimulator* CreateCodeSimulator(InstructionSet target_isa);

  virtual void RunFrom(intptr_t code_buffer) = 0;

  // Get return value according to C ABI.
  virtual bool GetCReturnBool() const = 0;
  virtual int32_t GetCReturnInt32() const = 0;
  virtual int64_t GetCReturnInt64() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeSimulator);
};

extern "C" CodeSimulator* CreateCodeSimulator(InstructionSet target_isa);

}  // namespace art

#endif  // ART_RUNTIME_SIMULATOR_CODE_SIMULATOR_H_
