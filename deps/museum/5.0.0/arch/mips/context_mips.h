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

#ifndef ART_RUNTIME_ARCH_MIPS_CONTEXT_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_CONTEXT_MIPS_H_

#include "arch/context.h"
#include "base/logging.h"
#include "registers_mips.h"

namespace art {
namespace mips {

class MipsContext : public Context {
 public:
  MipsContext() {
    Reset();
  }
  virtual ~MipsContext() {}

  void Reset() OVERRIDE;

  void FillCalleeSaves(const StackVisitor& fr) OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSP(uintptr_t new_sp) OVERRIDE {
    bool success = SetGPR(SP, new_sp);
    CHECK(success) << "Failed to set SP register";
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    bool success = SetGPR(RA, new_pc);
    CHECK(success) << "Failed to set RA register";
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    return gprs_[reg];
  }

  bool GetGPR(uint32_t reg, uintptr_t* val) OVERRIDE {
    CHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    if (gprs_[reg] == nullptr) {
      return false;
    } else {
      DCHECK(val != nullptr);
      *val = *gprs_[reg];
      return true;
    }
  }

  bool SetGPR(uint32_t reg, uintptr_t value) OVERRIDE;

  bool GetFPR(uint32_t reg, uintptr_t* val) OVERRIDE {
    CHECK_LT(reg, static_cast<uint32_t>(kNumberOfFRegisters));
    if (fprs_[reg] == nullptr) {
      return false;
    } else {
      DCHECK(val != nullptr);
      *val = *fprs_[reg];
      return true;
    }
  }

  bool SetFPR(uint32_t reg, uintptr_t value) OVERRIDE;

  void SmashCallerSaves() OVERRIDE;
  void DoLongJump() OVERRIDE;

 private:
  // Pointers to registers in the stack, initialized to NULL except for the special cases below.
  uintptr_t* gprs_[kNumberOfCoreRegisters];
  uint32_t* fprs_[kNumberOfFRegisters];
  // Hold values for sp and ra (return address) if they are not located within a stack frame.
  uintptr_t sp_, ra_;
};
}  // namespace mips
}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS_CONTEXT_MIPS_H_
