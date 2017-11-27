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

#ifndef ART_RUNTIME_VERIFIER_INSTRUCTION_FLAGS_H_
#define ART_RUNTIME_VERIFIER_INSTRUCTION_FLAGS_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"

namespace art {
namespace verifier {

class InstructionFlags FINAL {
 public:
  InstructionFlags() : flags_(0) {}

  void SetIsOpcode() {
    flags_ |= 1 << kOpcode;
  }
  void ClearIsOpcode() {
    flags_ &= ~(1 << kOpcode);
  }
  bool IsOpcode() const {
    return (flags_ & (1 << kOpcode)) != 0;
  }

  void SetInTry() {
    flags_ |= 1 << kInTry;
  }
  void ClearInTry() {
    flags_ &= ~(1 << kInTry);
  }
  bool IsInTry() const {
    return (flags_ & (1 << kInTry)) != 0;
  }

  void SetBranchTarget() {
    flags_ |= 1 << kBranchTarget;
  }
  void ClearBranchTarget() {
    flags_ &= ~(1 << kBranchTarget);
  }
  bool IsBranchTarget() const {
    return (flags_ & (1 << kBranchTarget)) != 0;
  }
  void SetCompileTimeInfoPoint() {
    flags_ |= 1 << kCompileTimeInfoPoint;
  }
  void ClearCompileTimeInfoPoint() {
    flags_ &= ~(1 << kCompileTimeInfoPoint);
  }
  bool IsCompileTimeInfoPoint() const {
    return (flags_ & (1 << kCompileTimeInfoPoint)) != 0;
  }

  void SetVisited() {
    flags_ |= 1 << kVisited;
  }
  void ClearVisited() {
    flags_ &= ~(1 << kVisited);
  }
  bool IsVisited() const {
    return (flags_ & (1 << kVisited)) != 0;
  }

  void SetChanged() {
    flags_ |= 1 << kChanged;
  }
  void ClearChanged() {
    flags_ &= ~(1 << kChanged);
  }
  bool IsChanged() const {
    return (flags_ & (1 << kChanged)) != 0;
  }

  bool IsVisitedOrChanged() const {
    return IsVisited() || IsChanged();
  }

  void SetReturn() {
    flags_ |= 1 << kReturn;
  }
  void ClearReturn() {
    flags_ &= ~(1 << kReturn);
  }
  bool IsReturn() const {
    return (flags_ & (1 << kReturn)) != 0;
  }

  void SetCompileTimeInfoPointAndReturn() {
    SetCompileTimeInfoPoint();
    SetReturn();
  }

  std::string ToString() const;

 private:
  enum {
    // The instruction has been visited and unless IsChanged() verified.
    kVisited = 0,
    // Register type information flowing into the instruction changed and so the instruction must be
    // reprocessed.
    kChanged = 1,
    // The item at this location is an opcode.
    kOpcode = 2,
    // Instruction is contained within a try region.
    kInTry = 3,
    // Instruction is the target of a branch (ie the start of a basic block).
    kBranchTarget = 4,
    // Location of interest to the compiler for GC maps and verifier based method sharpening.
    kCompileTimeInfoPoint = 5,
    // A return instruction.
    kReturn = 6,
  };
  uint8_t flags_;
};

static_assert(sizeof(InstructionFlags) == sizeof(uint8_t),
              "Size of InstructionFlags not equal to uint8_t");

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_INSTRUCTION_FLAGS_H_
