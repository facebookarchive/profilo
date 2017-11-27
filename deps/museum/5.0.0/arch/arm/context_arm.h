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

#ifndef ART_RUNTIME_ARCH_ARM_CONTEXT_ARM_H_
#define ART_RUNTIME_ARCH_ARM_CONTEXT_ARM_H_

#include "arch/context.h"
#include "base/logging.h"
#include "registers_arm.h"

namespace art {
namespace arm {

class ArmContext : public Context {
 public:
  ArmContext() {
    Reset();
  }

  virtual ~ArmContext() {}

  void Reset() OVERRIDE;

  void FillCalleeSaves(const StackVisitor& fr) OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSP(uintptr_t new_sp) OVERRIDE {
    bool success = SetGPR(SP, new_sp);
    CHECK(success) << "Failed to set SP register";
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    bool success = SetGPR(PC, new_pc);
    CHECK(success) << "Failed to set PC register";
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    return gprs_[reg];
  }

  bool GetGPR(uint32_t reg, uintptr_t* val) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
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
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfSRegisters));
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
  // Pointers to register locations, initialized to NULL or the specific registers below.
  uintptr_t* gprs_[kNumberOfCoreRegisters];
  uint32_t* fprs_[kNumberOfSRegisters];
  // Hold values for sp and pc if they are not located within a stack frame.
  uintptr_t sp_, pc_;
};

}  // namespace arm
}  // namespace art

#endif  // ART_RUNTIME_ARCH_ARM_CONTEXT_ARM_H_
