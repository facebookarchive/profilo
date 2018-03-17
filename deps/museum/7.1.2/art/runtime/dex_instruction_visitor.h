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

#ifndef ART_RUNTIME_DEX_INSTRUCTION_VISITOR_H_
#define ART_RUNTIME_DEX_INSTRUCTION_VISITOR_H_

#include "base/macros.h"
#include "dex_instruction.h"

namespace art {

template<typename T>
class DexInstructionVisitor {
 public:
  void Visit(const uint16_t* code, size_t size_in_bytes) {
    T* derived = static_cast<T*>(this);
    size_t size_in_code_units = size_in_bytes / sizeof(uint16_t);
    size_t i = 0;
    while (i < size_in_code_units) {
      const Instruction* inst = Instruction::At(&code[i]);
      switch (inst->Opcode()) {
#define INSTRUCTION_CASE(o, cname, p, f, r, i, a, v)  \
        case Instruction::cname: {                    \
          derived->Do_ ## cname(inst);                \
          break;                                      \
        }
#include "dex_instruction_list.h"
        DEX_INSTRUCTION_LIST(INSTRUCTION_CASE)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_CASE
        default:
          CHECK(false);
      }
      i += inst->SizeInCodeUnits();
    }
  }

 private:
  // Specific handlers for each instruction.
#define INSTRUCTION_VISITOR(o, cname, p, f, r, i, a, v)    \
  void Do_ ## cname(const Instruction* inst) {             \
    T* derived = static_cast<T*>(this);                    \
    derived->Do_Default(inst);                             \
  }
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_VISITOR)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_VISITOR

  // The default instruction handler.
  void Do_Default(const Instruction*) {
    return;
  }
};


}  // namespace art

#endif  // ART_RUNTIME_DEX_INSTRUCTION_VISITOR_H_
