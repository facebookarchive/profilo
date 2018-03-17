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
#include "base/macros.h"
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

  void FillCalleeSaves(uint8_t* frame, const QuickMethodFrameInfo& fr) OVERRIDE;

  void SetSP(uintptr_t new_sp) OVERRIDE {
    SetGPR(SP, new_sp);
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    SetGPR(PC, new_pc);
  }

  void SetArg0(uintptr_t new_arg0_value) OVERRIDE {
    SetGPR(R0, new_arg0_value);
  }

  bool IsAccessibleGPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    return gprs_[reg] != nullptr;
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    return gprs_[reg];
  }

  uintptr_t GetGPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
    DCHECK(IsAccessibleGPR(reg));
    return *gprs_[reg];
  }

  void SetGPR(uint32_t reg, uintptr_t value) OVERRIDE;

  bool IsAccessibleFPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfSRegisters));
    return fprs_[reg] != nullptr;
  }

  uintptr_t GetFPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfSRegisters));
    DCHECK(IsAccessibleFPR(reg));
    return *fprs_[reg];
  }

  void SetFPR(uint32_t reg, uintptr_t value) OVERRIDE;

  void SmashCallerSaves() OVERRIDE;
  NO_RETURN void DoLongJump() OVERRIDE;

 private:
  // Pointers to register locations, initialized to null or the specific registers below.
  uintptr_t* gprs_[kNumberOfCoreRegisters];
  uint32_t* fprs_[kNumberOfSRegisters];
  // Hold values for sp and pc if they are not located within a stack frame.
  uintptr_t sp_, pc_, arg0_;
};

}  // namespace arm
}  // namespace art

#endif  // ART_RUNTIME_ARCH_ARM_CONTEXT_ARM_H_
