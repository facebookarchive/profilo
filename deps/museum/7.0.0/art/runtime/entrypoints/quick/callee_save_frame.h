/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_

#include "arch/instruction_set.h"
#include "base/mutex.h"
#include "runtime.h"
#include "thread-inl.h"

// Specific frame size code is in architecture-specific files. We include this to compile-time
// specialize the code.
#include "arch/arm/quick_method_frame_info_arm.h"
#include "arch/arm64/quick_method_frame_info_arm64.h"
#include "arch/mips/quick_method_frame_info_mips.h"
#include "arch/mips64/quick_method_frame_info_mips64.h"
#include "arch/x86/quick_method_frame_info_x86.h"
#include "arch/x86_64/quick_method_frame_info_x86_64.h"

namespace art {
class ArtMethod;

class ScopedQuickEntrypointChecks {
 public:
  explicit ScopedQuickEntrypointChecks(Thread *self,
                                       bool entry_check = kIsDebugBuild,
                                       bool exit_check = kIsDebugBuild)
      SHARED_REQUIRES(Locks::mutator_lock_) : self_(self), exit_check_(exit_check) {
    if (entry_check) {
      TestsOnEntry();
    }
  }

  ScopedQuickEntrypointChecks() SHARED_REQUIRES(Locks::mutator_lock_)
      : self_(kIsDebugBuild ? Thread::Current() : nullptr), exit_check_(kIsDebugBuild) {
    if (kIsDebugBuild) {
      TestsOnEntry();
    }
  }

  ~ScopedQuickEntrypointChecks() SHARED_REQUIRES(Locks::mutator_lock_) {
    if (exit_check_) {
      TestsOnExit();
    }
  }

 private:
  void TestsOnEntry() SHARED_REQUIRES(Locks::mutator_lock_) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
    self_->VerifyStack();
  }

  void TestsOnExit() SHARED_REQUIRES(Locks::mutator_lock_) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
    self_->VerifyStack();
  }

  Thread* const self_;
  bool exit_check_;
};

static constexpr size_t GetCalleeSaveFrameSize(InstructionSet isa, Runtime::CalleeSaveType type) {
  // constexpr must be a return statement.
  return (isa == kArm || isa == kThumb2) ? arm::ArmCalleeSaveFrameSize(type) :
         isa == kArm64 ? arm64::Arm64CalleeSaveFrameSize(type) :
         isa == kMips ? mips::MipsCalleeSaveFrameSize(type) :
         isa == kMips64 ? mips64::Mips64CalleeSaveFrameSize(type) :
         isa == kX86 ? x86::X86CalleeSaveFrameSize(type) :
         isa == kX86_64 ? x86_64::X86_64CalleeSaveFrameSize(type) :
         isa == kNone ? (LOG(FATAL) << "kNone has no frame size", 0) :
         (LOG(FATAL) << "Unknown instruction set" << isa, 0);
}

// Note: this specialized statement is sanity-checked in the quick-trampoline gtest.
static constexpr size_t GetConstExprPointerSize(InstructionSet isa) {
  // constexpr must be a return statement.
  return (isa == kArm || isa == kThumb2) ? kArmPointerSize :
         isa == kArm64 ? kArm64PointerSize :
         isa == kMips ? kMipsPointerSize :
         isa == kMips64 ? kMips64PointerSize :
         isa == kX86 ? kX86PointerSize :
         isa == kX86_64 ? kX86_64PointerSize :
         isa == kNone ? (LOG(FATAL) << "kNone has no pointer size", 0) :
         (LOG(FATAL) << "Unknown instruction set" << isa, 0);
}

// Note: this specialized statement is sanity-checked in the quick-trampoline gtest.
static constexpr size_t GetCalleeSaveReturnPcOffset(InstructionSet isa,
                                                    Runtime::CalleeSaveType type) {
  return GetCalleeSaveFrameSize(isa, type) - GetConstExprPointerSize(isa);
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
