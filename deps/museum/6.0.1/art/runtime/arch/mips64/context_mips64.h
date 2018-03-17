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

#ifndef ART_RUNTIME_ARCH_MIPS64_CONTEXT_MIPS64_H_
#define ART_RUNTIME_ARCH_MIPS64_CONTEXT_MIPS64_H_

#include "arch/context.h"
#include "base/logging.h"
#include "base/macros.h"
#include "registers_mips64.h"

namespace art {
namespace mips64 {

class Mips64Context : public Context {
 public:
  Mips64Context() {
    Reset();
  }
  virtual ~Mips64Context() {}

  void Reset() OVERRIDE;

  void FillCalleeSaves(const StackVisitor& fr) OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSP(uintptr_t new_sp) OVERRIDE {
    SetGPR(SP, new_sp);
  }

  void SetPC(uintptr_t new_pc) OVERRIDE {
    SetGPR(RA, new_pc);
  }

  bool IsAccessibleGPR(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfGpuRegisters));
    return gprs_[reg] != nullptr;
  }

  uintptr_t* GetGPRAddress(uint32_t reg) OVERRIDE {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfGpuRegisters));
    return gprs_[reg];
  }

  uintptr_t GetGPR(uint32_t reg) OVERRIDE {
    CHECK_LT(reg, static_cast<uint32_t>(kNumberOfGpuRegisters));
    DCHECK(IsAccessibleGPR(reg));
    return *gprs_[reg];
  }

  void SetGPR(uint32_t reg, uintptr_t value) OVERRIDE;

  bool IsAccessibleFPR(uint32_t reg) OVERRIDE {
    CHECK_LT(reg, static_cast<uint32_t>(kNumberOfFpuRegisters));
    return fprs_[reg] != nullptr;
  }

  uintptr_t GetFPR(uint32_t reg) OVERRIDE {
    CHECK_LT(reg, static_cast<uint32_t>(kNumberOfFpuRegisters));
    DCHECK(IsAccessibleFPR(reg));
    return *fprs_[reg];
  }

  void SetFPR(uint32_t reg, uintptr_t value) OVERRIDE;

  void SmashCallerSaves() OVERRIDE;
  NO_RETURN void DoLongJump() OVERRIDE;

 private:
  // Pointers to registers in the stack, initialized to null except for the special cases below.
  uintptr_t* gprs_[kNumberOfGpuRegisters];
  uint64_t* fprs_[kNumberOfFpuRegisters];
  // Hold values for sp and ra (return address) if they are not located within a stack frame.
  uintptr_t sp_, ra_;
};
}  // namespace mips64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS64_CONTEXT_MIPS64_H_
