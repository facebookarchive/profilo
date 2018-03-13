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

#ifndef ART_RUNTIME_ARCH_X86_64_CONTEXT_X86_64_H_
#define ART_RUNTIME_ARCH_X86_64_CONTEXT_X86_64_H_

#include "arch/context.h"
#include "base/logging.h"
#include "base/macros.h"
#include "registers_x86_64.h"

namespace art {
namespace x86_64 {

class X86_64Context : public Context {
 public:
  X86_64Context() {
    Reset();
  }
  virtual ~X86_64Context() {}

  void Reset() OVERRIDE;

  void FillCalleeSaves(uint8_t* frame, const QuickMethodFrameInfo& fr) OVERRIDE;

  void SetSP(uintptr_t new_sp) OVERRIDE {
    SetGPR(RSP, new_sp);
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    rip_ = new_pc;
  }

  void SetArg0(uintptr_t new_arg0_value) OVERRIDE {
    SetGPR(RDI, new_arg0_value);
  }

  bool IsAccessibleGPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    return gprs_[reg] != nullptr;
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    return gprs_[reg];
  }

  uintptr_t GetGPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    DCHECK(IsAccessibleGPR(reg));
    return *gprs_[reg];
  }

  void SetGPR(uint32_t reg, uintptr_t value) OVERRIDE;

  bool IsAccessibleFPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfFloatRegisters));
    return fprs_[reg] != nullptr;
  }

  uintptr_t GetFPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfFloatRegisters));
    DCHECK(IsAccessibleFPR(reg));
    return *fprs_[reg];
  }

  void SetFPR(uint32_t reg, uintptr_t value) OVERRIDE;

  void SmashCallerSaves() OVERRIDE;
  NO_RETURN void DoLongJump() OVERRIDE;

 private:
  // Pointers to register locations. Values are initialized to null or the special registers below.
  uintptr_t* gprs_[kNumberOfCpuRegisters];
  uint64_t* fprs_[kNumberOfFloatRegisters];
  // Hold values for rsp, rip and arg0 if they are not located within a stack frame. RIP is somewhat
  // special in that it cannot be encoded normally as a register operand to an instruction (except
  // in 64bit addressing modes).
  uintptr_t rsp_, rip_, arg0_;
};
}  // namespace x86_64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_64_CONTEXT_X86_64_H_
