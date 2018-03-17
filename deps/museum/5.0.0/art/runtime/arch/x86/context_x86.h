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

#ifndef ART_RUNTIME_ARCH_X86_CONTEXT_X86_H_
#define ART_RUNTIME_ARCH_X86_CONTEXT_X86_H_

#include "arch/context.h"
#include "base/logging.h"
#include "registers_x86.h"

namespace art {
namespace x86 {

class X86Context : public Context {
 public:
  X86Context() {
    Reset();
  }
  virtual ~X86Context() {}

  void Reset() OVERRIDE;

  void FillCalleeSaves(const StackVisitor& fr) OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSP(uintptr_t new_sp) OVERRIDE {
    bool success = SetGPR(ESP, new_sp);
    CHECK(success) << "Failed to set ESP register";
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    eip_ = new_pc;
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    return gprs_[reg];
  }

  bool GetGPR(uint32_t reg, uintptr_t* val) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
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
    LOG(FATAL) << "Floating-point registers are all caller save in X86";
    return false;
  }

  bool SetFPR(uint32_t reg, uintptr_t value) OVERRIDE {
    LOG(FATAL) << "Floating-point registers are all caller save in X86";
    return false;
  }

  void SmashCallerSaves() OVERRIDE;
  void DoLongJump() OVERRIDE;

 private:
  // Pointers to register locations, floating point registers are all caller save. Values are
  // initialized to NULL or the special registers below.
  uintptr_t* gprs_[kNumberOfCpuRegisters];
  // Hold values for esp and eip if they are not located within a stack frame. EIP is somewhat
  // special in that it cannot be encoded normally as a register operand to an instruction (except
  // in 64bit addressing modes).
  uintptr_t esp_, eip_;
};
}  // namespace x86
}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_CONTEXT_X86_H_
