/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_ARM64_CONTEXT_ARM64_H_
#define ART_RUNTIME_ARCH_ARM64_CONTEXT_ARM64_H_

#include "arch/context.h"
#include "base/logging.h"
#include "base/macros.h"
#include "registers_arm64.h"

namespace art {
namespace arm64 {

class Arm64Context : public Context {
 public:
  Arm64Context() {
    Reset();
  }

  ~Arm64Context() {}

  void Reset() OVERRIDE;

  void FillCalleeSaves(uint8_t* frame, const QuickMethodFrameInfo& fr) OVERRIDE;

  void SetSP(uintptr_t new_sp) OVERRIDE {
    SetGPR(SP, new_sp);
  }

  void SetPC(uintptr_t new_lr) OVERRIDE {
    SetGPR(kPC, new_lr);
  }

  void SetArg0(uintptr_t new_arg0_value) OVERRIDE {
    SetGPR(X0, new_arg0_value);
  }

  bool IsAccessibleGPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, arraysize(gprs_));
    return gprs_[reg] != nullptr;
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, arraysize(gprs_));
    return gprs_[reg];
  }

  uintptr_t GetGPR(uint32_t reg) OVERRIDE {
    // Note: PC isn't an available GPR (outside of internals), so don't allow retrieving the value.
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfXRegisters));
    DCHECK(IsAccessibleGPR(reg));
    return *gprs_[reg];
  }

  void SetGPR(uint32_t reg, uintptr_t value) OVERRIDE;

  bool IsAccessibleFPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfDRegisters));
    return fprs_[reg] != nullptr;
  }

  uintptr_t GetFPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfDRegisters));
    DCHECK(IsAccessibleFPR(reg));
    return *fprs_[reg];
  }

  void SetFPR(uint32_t reg, uintptr_t value) OVERRIDE;

  void SmashCallerSaves() OVERRIDE;
  NO_RETURN void DoLongJump() OVERRIDE;

  static constexpr size_t kPC = kNumberOfXRegisters;

 private:
  // Pointers to register locations, initialized to null or the specific registers below. We need
  // an additional one for the PC.
  uintptr_t* gprs_[kNumberOfXRegisters + 1];
  uint64_t * fprs_[kNumberOfDRegisters];
  // Hold values for sp, pc and arg0 if they are not located within a stack frame.
  uintptr_t sp_, pc_, arg0_;
};

}  // namespace arm64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_ARM64_CONTEXT_ARM64_H_
