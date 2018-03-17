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

  void FillCalleeSaves(const StackVisitor& fr) OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSP(uintptr_t new_sp) OVERRIDE {
    bool success = SetGPR(RSP, new_sp);
    CHECK(success) << "Failed to set RSP register";
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    rip_ = new_pc;
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
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfFloatRegisters));
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
  // Pointers to register locations. Values are initialized to NULL or the special registers below.
  uintptr_t* gprs_[kNumberOfCpuRegisters];
  uint64_t* fprs_[kNumberOfFloatRegisters];
  // Hold values for rsp and rip if they are not located within a stack frame. RIP is somewhat
  // special in that it cannot be encoded normally as a register operand to an instruction (except
  // in 64bit addressing modes).
  uintptr_t rsp_, rip_;
};
}  // namespace x86_64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_64_CONTEXT_X86_64_H_
