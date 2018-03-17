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

#ifndef ART_RUNTIME_QUICKEN_INFO_H_
#define ART_RUNTIME_QUICKEN_INFO_H_

#include "dex_instruction.h"

namespace art {

// QuickenInfoTable is a table of 16 bit dex indices. There is one slot fo every instruction that is
// possibly dequickenable.
class QuickenInfoTable {
 public:
  explicit QuickenInfoTable(const uint8_t* data) : data_(data) {}

  bool IsNull() const {
    return data_ == nullptr;
  }

  uint16_t GetData(size_t index) const {
    return data_[index * 2] | (static_cast<uint16_t>(data_[index * 2 + 1]) << 8);
  }

  // Returns true if the dex instruction has an index in the table. (maybe dequickenable).
  static bool NeedsIndexForInstruction(const Instruction* inst) {
    return inst->IsQuickened() || inst->Opcode() == Instruction::NOP;
  }

  static size_t NumberOfIndices(size_t bytes) {
    return bytes / sizeof(uint16_t);
  }

 private:
  const uint8_t* const data_;

  DISALLOW_COPY_AND_ASSIGN(QuickenInfoTable);
};

}  // namespace art

#endif  // ART_RUNTIME_QUICKEN_INFO_H_
