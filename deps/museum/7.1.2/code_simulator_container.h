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

#ifndef ART_RUNTIME_CODE_SIMULATOR_CONTAINER_H_
#define ART_RUNTIME_CODE_SIMULATOR_CONTAINER_H_

#include "arch/instruction_set.h"
#include "simulator/code_simulator.h"

namespace art {

// This container dynamically opens and closes libart-simulator.
class CodeSimulatorContainer {
 public:
  explicit CodeSimulatorContainer(InstructionSet target_isa);
  ~CodeSimulatorContainer();

  bool CanSimulate() const {
    return simulator_ != nullptr;
  }

  CodeSimulator* Get() {
    DCHECK(CanSimulate());
    return simulator_;
  }

  const CodeSimulator* Get() const {
    DCHECK(CanSimulate());
    return simulator_;
  }

 private:
  void* libart_simulator_handle_;
  CodeSimulator* simulator_;

  DISALLOW_COPY_AND_ASSIGN(CodeSimulatorContainer);
};

}  // namespace art

#endif  // ART_RUNTIME_CODE_SIMULATOR_CONTAINER_H_
