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

#ifndef ART_RUNTIME_ARCH_ARM64_QUICK_METHOD_FRAME_INFO_ARM64_H_
#define ART_RUNTIME_ARCH_ARM64_QUICK_METHOD_FRAME_INFO_ARM64_H_

#include "base/bit_utils.h"
#include "quick/quick_method_frame_info.h"
#include "registers_arm64.h"
#include "runtime.h"  // for Runtime::CalleeSaveType.

namespace art {
namespace arm64 {

// Registers need to be restored but not preserved by aapcs64.
static constexpr uint32_t kArm64CalleeSaveAlwaysSpills =
    // Note: ArtMethod::GetReturnPcOffsetInBytes() rely on the assumption that
    // LR is always saved on the top of the frame for all targets.
    // That is, lr = *(sp + framesize - pointer_size).
    (1 << art::arm64::LR);
// Callee saved registers
static constexpr uint32_t kArm64CalleeSaveRefSpills =
    (1 << art::arm64::X20) | (1 << art::arm64::X21) | (1 << art::arm64::X22) |
    (1 << art::arm64::X23) | (1 << art::arm64::X24) | (1 << art::arm64::X25) |
    (1 << art::arm64::X26) | (1 << art::arm64::X27) | (1 << art::arm64::X28) |
    (1 << art::arm64::X29);
// X0 is the method pointer. Not saved.
static constexpr uint32_t kArm64CalleeSaveArgSpills =
    (1 << art::arm64::X1) | (1 << art::arm64::X2) | (1 << art::arm64::X3) |
    (1 << art::arm64::X4) | (1 << art::arm64::X5) | (1 << art::arm64::X6) |
    (1 << art::arm64::X7);
static constexpr uint32_t kArm64CalleeSaveAllSpills =
    (1 << art::arm64::X19);
static constexpr uint32_t kArm64CalleeSaveEverythingSpills =
    (1 << art::arm64::X0) | (1 << art::arm64::X1) | (1 << art::arm64::X2) |
    (1 << art::arm64::X3) | (1 << art::arm64::X4) | (1 << art::arm64::X5) |
    (1 << art::arm64::X6) | (1 << art::arm64::X7) | (1 << art::arm64::X8) |
    (1 << art::arm64::X9) | (1 << art::arm64::X10) | (1 << art::arm64::X11) |
    (1 << art::arm64::X12) | (1 << art::arm64::X13) | (1 << art::arm64::X14) |
    (1 << art::arm64::X15) | (1 << art::arm64::X16) | (1 << art::arm64::X17) |
    (1 << art::arm64::X18) | (1 << art::arm64::X19);

static constexpr uint32_t kArm64CalleeSaveFpAlwaysSpills = 0;
static constexpr uint32_t kArm64CalleeSaveFpRefSpills = 0;
static constexpr uint32_t kArm64CalleeSaveFpArgSpills =
    (1 << art::arm64::D0) | (1 << art::arm64::D1) | (1 << art::arm64::D2) |
    (1 << art::arm64::D3) | (1 << art::arm64::D4) | (1 << art::arm64::D5) |
    (1 << art::arm64::D6) | (1 << art::arm64::D7);
static constexpr uint32_t kArm64CalleeSaveFpAllSpills =
    (1 << art::arm64::D8)  | (1 << art::arm64::D9)  | (1 << art::arm64::D10) |
    (1 << art::arm64::D11)  | (1 << art::arm64::D12)  | (1 << art::arm64::D13) |
    (1 << art::arm64::D14)  | (1 << art::arm64::D15);
static constexpr uint32_t kArm64CalleeSaveFpEverythingSpills =
    (1 << art::arm64::D0) | (1 << art::arm64::D1) | (1 << art::arm64::D2) |
    (1 << art::arm64::D3) | (1 << art::arm64::D4) | (1 << art::arm64::D5) |
    (1 << art::arm64::D6) | (1 << art::arm64::D7) | (1 << art::arm64::D8) |
    (1 << art::arm64::D9) | (1 << art::arm64::D10) | (1 << art::arm64::D11) |
    (1 << art::arm64::D12) | (1 << art::arm64::D13) | (1 << art::arm64::D14) |
    (1 << art::arm64::D15) | (1 << art::arm64::D16) | (1 << art::arm64::D17) |
    (1 << art::arm64::D18) | (1 << art::arm64::D19) | (1 << art::arm64::D20) |
    (1 << art::arm64::D21) | (1 << art::arm64::D22) | (1 << art::arm64::D23) |
    (1 << art::arm64::D24) | (1 << art::arm64::D25) | (1 << art::arm64::D26) |
    (1 << art::arm64::D27) | (1 << art::arm64::D28) | (1 << art::arm64::D29) |
    (1 << art::arm64::D30) | (1 << art::arm64::D31);

constexpr uint32_t Arm64CalleeSaveCoreSpills(Runtime::CalleeSaveType type) {
  return kArm64CalleeSaveAlwaysSpills | kArm64CalleeSaveRefSpills |
      (type == Runtime::kSaveRefsAndArgs ? kArm64CalleeSaveArgSpills : 0) |
      (type == Runtime::kSaveAllCalleeSaves ? kArm64CalleeSaveAllSpills : 0) |
      (type == Runtime::kSaveEverything ? kArm64CalleeSaveEverythingSpills : 0);
}

constexpr uint32_t Arm64CalleeSaveFpSpills(Runtime::CalleeSaveType type) {
  return kArm64CalleeSaveFpAlwaysSpills | kArm64CalleeSaveFpRefSpills |
      (type == Runtime::kSaveRefsAndArgs ? kArm64CalleeSaveFpArgSpills : 0) |
      (type == Runtime::kSaveAllCalleeSaves ? kArm64CalleeSaveFpAllSpills : 0) |
      (type == Runtime::kSaveEverything ? kArm64CalleeSaveFpEverythingSpills : 0);
}

constexpr uint32_t Arm64CalleeSaveFrameSize(Runtime::CalleeSaveType type) {
  return RoundUp((POPCOUNT(Arm64CalleeSaveCoreSpills(type)) /* gprs */ +
                  POPCOUNT(Arm64CalleeSaveFpSpills(type)) /* fprs */ +
                  1 /* Method* */) * static_cast<size_t>(kArm64PointerSize), kStackAlignment);
}

constexpr QuickMethodFrameInfo Arm64CalleeSaveMethodFrameInfo(Runtime::CalleeSaveType type) {
  return QuickMethodFrameInfo(Arm64CalleeSaveFrameSize(type),
                              Arm64CalleeSaveCoreSpills(type),
                              Arm64CalleeSaveFpSpills(type));
}

constexpr size_t Arm64CalleeSaveFpr1Offset(Runtime::CalleeSaveType type) {
  return Arm64CalleeSaveFrameSize(type) -
         (POPCOUNT(Arm64CalleeSaveCoreSpills(type)) +
          POPCOUNT(Arm64CalleeSaveFpSpills(type))) * static_cast<size_t>(kArm64PointerSize);
}

constexpr size_t Arm64CalleeSaveGpr1Offset(Runtime::CalleeSaveType type) {
  return Arm64CalleeSaveFrameSize(type) -
         POPCOUNT(Arm64CalleeSaveCoreSpills(type)) * static_cast<size_t>(kArm64PointerSize);
}

constexpr size_t Arm64CalleeSaveLrOffset(Runtime::CalleeSaveType type) {
  return Arm64CalleeSaveFrameSize(type) -
      POPCOUNT(Arm64CalleeSaveCoreSpills(type) & (-(1 << LR))) *
      static_cast<size_t>(kArm64PointerSize);
}

}  // namespace arm64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_ARM64_QUICK_METHOD_FRAME_INFO_ARM64_H_
