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

#ifndef ART_RUNTIME_ARCH_ARM_QUICK_METHOD_FRAME_INFO_ARM_H_
#define ART_RUNTIME_ARCH_ARM_QUICK_METHOD_FRAME_INFO_ARM_H_

#include <museum/8.0.0/art/runtime/base/bit_utils.h>
#include <museum/8.0.0/art/runtime/quick/quick_method_frame_info.h>
#include <museum/8.0.0/art/runtime/arch/arm/registers_arm.h>
#include <museum/8.0.0/art/runtime/runtime.h>  // for Runtime::CalleeSaveType.

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace arm {

static constexpr uint32_t kArmCalleeSaveAlwaysSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::LR);
static constexpr uint32_t kArmCalleeSaveRefSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::R5) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R6)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R7) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R8) |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::R10) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R11);
static constexpr uint32_t kArmCalleeSaveArgSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::R1) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R2) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R3);
static constexpr uint32_t kArmCalleeSaveAllSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::R4) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R9);
static constexpr uint32_t kArmCalleeSaveEverythingSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::R0) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R1) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R2) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R3) |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::R4) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R9) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::R12);

static constexpr uint32_t kArmCalleeSaveFpAlwaysSpills = 0;
static constexpr uint32_t kArmCalleeSaveFpRefSpills = 0;
static constexpr uint32_t kArmCalleeSaveFpArgSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S0)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S1)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S2)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S3)  |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S4)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S5)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S6)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S7)  |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S8)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S9)  | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S10) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S11) |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S12) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S13) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S14) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S15);
static constexpr uint32_t kArmCalleeSaveFpAllSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S16) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S17) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S18) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S19) |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S20) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S21) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S22) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S23) |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S24) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S25) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S26) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S27) |
    (1 << facebook::museum::MUSEUM_VERSION::art::arm::S28) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S29) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S30) | (1 << facebook::museum::MUSEUM_VERSION::art::arm::S31);
static constexpr uint32_t kArmCalleeSaveFpEverythingSpills =
    kArmCalleeSaveFpArgSpills | kArmCalleeSaveFpAllSpills;

constexpr uint32_t ArmCalleeSaveCoreSpills(Runtime::CalleeSaveType type) {
  return kArmCalleeSaveAlwaysSpills | kArmCalleeSaveRefSpills |
      (type == Runtime::kSaveRefsAndArgs ? kArmCalleeSaveArgSpills : 0) |
      (type == Runtime::kSaveAllCalleeSaves ? kArmCalleeSaveAllSpills : 0) |
      (type == Runtime::kSaveEverything ? kArmCalleeSaveEverythingSpills : 0);
}

constexpr uint32_t ArmCalleeSaveFpSpills(Runtime::CalleeSaveType type) {
  return kArmCalleeSaveFpAlwaysSpills | kArmCalleeSaveFpRefSpills |
      (type == Runtime::kSaveRefsAndArgs ? kArmCalleeSaveFpArgSpills : 0) |
      (type == Runtime::kSaveAllCalleeSaves ? kArmCalleeSaveFpAllSpills : 0) |
      (type == Runtime::kSaveEverything ? kArmCalleeSaveFpEverythingSpills : 0);
}

constexpr uint32_t ArmCalleeSaveFrameSize(Runtime::CalleeSaveType type) {
  return RoundUp((POPCOUNT(ArmCalleeSaveCoreSpills(type)) /* gprs */ +
                  POPCOUNT(ArmCalleeSaveFpSpills(type)) /* fprs */ +
                  1 /* Method* */) * static_cast<size_t>(kArmPointerSize), kStackAlignment);
}

constexpr QuickMethodFrameInfo ArmCalleeSaveMethodFrameInfo(Runtime::CalleeSaveType type) {
  return QuickMethodFrameInfo(ArmCalleeSaveFrameSize(type),
                              ArmCalleeSaveCoreSpills(type),
                              ArmCalleeSaveFpSpills(type));
}

constexpr size_t ArmCalleeSaveFpr1Offset(Runtime::CalleeSaveType type) {
  return ArmCalleeSaveFrameSize(type) -
         (POPCOUNT(ArmCalleeSaveCoreSpills(type)) +
          POPCOUNT(ArmCalleeSaveFpSpills(type))) * static_cast<size_t>(kArmPointerSize);
}

constexpr size_t ArmCalleeSaveGpr1Offset(Runtime::CalleeSaveType type) {
  return ArmCalleeSaveFrameSize(type) -
         POPCOUNT(ArmCalleeSaveCoreSpills(type)) * static_cast<size_t>(kArmPointerSize);
}

constexpr size_t ArmCalleeSaveLrOffset(Runtime::CalleeSaveType type) {
  return ArmCalleeSaveFrameSize(type) -
      POPCOUNT(ArmCalleeSaveCoreSpills(type) & (-(1 << LR))) * static_cast<size_t>(kArmPointerSize);
}

}  // namespace arm
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_ARCH_ARM_QUICK_METHOD_FRAME_INFO_ARM_H_
