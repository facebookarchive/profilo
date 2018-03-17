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

#include <museum/7.0.0/art/runtime/base/bit_utils.h>
#include <museum/7.0.0/art/runtime/quick/quick_method_frame_info.h>
#include <museum/7.0.0/art/runtime/arch/mips/registers_mips.h>
#include <museum/7.0.0/art/runtime/runtime.h>  // for Runtime::CalleeSaveType.

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace mips {

static constexpr uint32_t kMipsCalleeSaveAlwaysSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::RA);
static constexpr uint32_t kMipsCalleeSaveRefSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S3) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S4) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S5) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S6) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S7) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::GP) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::FP);
static constexpr uint32_t kMipsCalleeSaveArgSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::A1) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A2) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::A3);
static constexpr uint32_t kMipsCalleeSaveAllSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::S0) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::S1);

static constexpr uint32_t kMipsCalleeSaveFpAlwaysSpills = 0;
static constexpr uint32_t kMipsCalleeSaveFpRefSpills = 0;
static constexpr uint32_t kMipsCalleeSaveFpArgSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F12) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F13) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F14) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F15);
static constexpr uint32_t kMipsCalleeSaveAllFPSpills =
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F20) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F21) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F22) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F23) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F24) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F25) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F26) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F27) |
    (1 << facebook::museum::MUSEUM_VERSION::art::mips::F28) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F29) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F30) | (1 << facebook::museum::MUSEUM_VERSION::art::mips::F31);

constexpr uint32_t MipsCalleeSaveCoreSpills(Runtime::CalleeSaveType type) {
  return kMipsCalleeSaveAlwaysSpills | kMipsCalleeSaveRefSpills |
      (type == Runtime::kRefsAndArgs ? kMipsCalleeSaveArgSpills : 0) |
      (type == Runtime::kSaveAll ? kMipsCalleeSaveAllSpills : 0);
}

constexpr uint32_t MipsCalleeSaveFPSpills(Runtime::CalleeSaveType type) {
  return kMipsCalleeSaveFpAlwaysSpills | kMipsCalleeSaveFpRefSpills |
      (type == Runtime::kRefsAndArgs ? kMipsCalleeSaveFpArgSpills : 0) |
      (type == Runtime::kSaveAll ? kMipsCalleeSaveAllFPSpills : 0);
}

constexpr uint32_t MipsCalleeSaveFrameSize(Runtime::CalleeSaveType type) {
  return RoundUp((POPCOUNT(MipsCalleeSaveCoreSpills(type)) /* gprs */ +
                  POPCOUNT(MipsCalleeSaveFPSpills(type))   /* fprs */ +
                  1 /* Method* */) * kMipsPointerSize, kStackAlignment);
}

constexpr QuickMethodFrameInfo MipsCalleeSaveMethodFrameInfo(Runtime::CalleeSaveType type) {
  return QuickMethodFrameInfo(MipsCalleeSaveFrameSize(type),
                              MipsCalleeSaveCoreSpills(type),
                              MipsCalleeSaveFPSpills(type));
}

}  // namespace mips
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_ARCH_MIPS_QUICK_METHOD_FRAME_INFO_MIPS_H_
