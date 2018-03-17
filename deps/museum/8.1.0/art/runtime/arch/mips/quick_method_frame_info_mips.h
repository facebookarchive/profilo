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

#ifndef ART_RUNTIME_ARCH_MIPS_QUICK_METHOD_FRAME_INFO_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_QUICK_METHOD_FRAME_INFO_MIPS_H_

#include <museum/8.1.0/art/runtime/arch/instruction_set.h>
#include <museum/8.1.0/art/runtime/base/bit_utils.h>
#include <museum/8.1.0/art/runtime/base/callee_save_type.h>
#include <museum/8.1.0/art/runtime/base/enums.h>
#include <museum/8.1.0/art/runtime/quick/quick_method_frame_info.h>
#include <museum/8.1.0/art/runtime/arch/mips/registers_mips.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace mips {

static constexpr uint32_t kMipsCalleeSaveAlwaysSpills =
    (1u << facebook::museum::MUSEUM_VERSION::art::mips::RA);
static constexpr uint32_t kMipsCalleeSaveRefSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S3) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S4) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S5) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S6) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S7) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::GP) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::FP);
static constexpr uint32_t kMipsCalleeSaveArgSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::A1) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A3) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T0) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::T1);
static constexpr uint32_t kMipsCalleeSaveAllSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S1);
static constexpr uint32_t kMipsCalleeSaveEverythingSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::AT) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::V0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::V1) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::A0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A1) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A3) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::T0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T1) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T3) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::T4) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T5) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T6) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T7) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S1) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T8) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::T9);

static constexpr uint32_t kMipsCalleeSaveFpAlwaysSpills = 0;
static constexpr uint32_t kMipsCalleeSaveFpRefSpills = 0;
static constexpr uint32_t kMipsCalleeSaveFpArgSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F8) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F9) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F10) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F11) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F12) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F13) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F14) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F15) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F16) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F17) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F18) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F19);
static constexpr uint32_t kMipsCalleeSaveAllFPSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F20) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F21) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F22) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F23) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F24) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F25) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F26) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F27) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F28) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F29) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F30) | (1u << facebook::museum::MUSEUM_VERSION::art::mips::F31);
static constexpr uint32_t kMipsCalleeSaveFpEverythingSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F1) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F3) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F4) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F5) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F6) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F7) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F8) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F9) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F10) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F11) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F12) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F13) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F14) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F15) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F16) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F17) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F18) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F19) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F20) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F21) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F22) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F23) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F24) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F25) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F26) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F27) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F28) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F29) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F30) | (1u << facebook::museum::MUSEUM_VERSION::art::mips::F31);

constexpr uint32_t MipsCalleeSaveCoreSpills(CalleeSaveType type) {
  return kMipsCalleeSaveAlwaysSpills | kMipsCalleeSaveRefSpills |
      (type == CalleeSaveType::kSaveRefsAndArgs ? kMipsCalleeSaveArgSpills : 0) |
      (type == CalleeSaveType::kSaveAllCalleeSaves ? kMipsCalleeSaveAllSpills : 0) |
      (type == CalleeSaveType::kSaveEverything ? kMipsCalleeSaveEverythingSpills : 0);
}

constexpr uint32_t MipsCalleeSaveFPSpills(CalleeSaveType type) {
  return kMipsCalleeSaveFpAlwaysSpills | kMipsCalleeSaveFpRefSpills |
      (type == CalleeSaveType::kSaveRefsAndArgs ? kMipsCalleeSaveFpArgSpills : 0) |
      (type == CalleeSaveType::kSaveAllCalleeSaves ? kMipsCalleeSaveAllFPSpills : 0) |
      (type == CalleeSaveType::kSaveEverything ? kMipsCalleeSaveFpEverythingSpills : 0);
}

constexpr uint32_t MipsCalleeSaveFrameSize(CalleeSaveType type) {
  return RoundUp((POPCOUNT(MipsCalleeSaveCoreSpills(type)) /* gprs */ +
                  POPCOUNT(MipsCalleeSaveFPSpills(type))   /* fprs */ +
                  1 /* Method* */) * static_cast<size_t>(kMipsPointerSize), kStackAlignment);
}

constexpr QuickMethodFrameInfo MipsCalleeSaveMethodFrameInfo(CalleeSaveType type) {
  return QuickMethodFrameInfo(MipsCalleeSaveFrameSize(type),
                              MipsCalleeSaveCoreSpills(type),
                              MipsCalleeSaveFPSpills(type));
}

}  // namespace mips
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_ARCH_MIPS_QUICK_METHOD_FRAME_INFO_MIPS_H_
