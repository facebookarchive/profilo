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

#ifndef ART_RUNTIME_ARCH_MIPS64_QUICK_METHOD_FRAME_INFO_MIPS64_H_
#define ART_RUNTIME_ARCH_MIPS64_QUICK_METHOD_FRAME_INFO_MIPS64_H_

#include "base/bit_utils.h"
#include "quick/quick_method_frame_info.h"
#include "registers_mips64.h"
#include "runtime.h"  // for Runtime::CalleeSaveType.

namespace art {
namespace mips64 {

static constexpr uint32_t kMips64CalleeSaveAlwaysSpills =
    (1 << art::mips64::RA);
static constexpr uint32_t kMips64CalleeSaveRefSpills =
    (1 << art::mips64::S2) | (1 << art::mips64::S3) | (1 << art::mips64::S4) |
    (1 << art::mips64::S5) | (1 << art::mips64::S6) | (1 << art::mips64::S7) |
    (1 << art::mips64::GP) | (1 << art::mips64::S8);
static constexpr uint32_t kMips64CalleeSaveArgSpills =
    (1 << art::mips64::A1) | (1 << art::mips64::A2) | (1 << art::mips64::A3) |
    (1 << art::mips64::A4) | (1 << art::mips64::A5) | (1 << art::mips64::A6) |
    (1 << art::mips64::A7);
static constexpr uint32_t kMips64CalleeSaveAllSpills =
    (1 << art::mips64::S0) | (1 << art::mips64::S1);
static constexpr uint32_t kMips64CalleeSaveEverythingSpills =
    (1 << art::mips64::AT) | (1 << art::mips64::V0) | (1 << art::mips64::V1) |
    (1 << art::mips64::A0) | (1 << art::mips64::A1) | (1 << art::mips64::A2) |
    (1 << art::mips64::A3) | (1 << art::mips64::A4) | (1 << art::mips64::A5) |
    (1 << art::mips64::A6) | (1 << art::mips64::A7) | (1 << art::mips64::T0) |
    (1 << art::mips64::T1) | (1 << art::mips64::T2) | (1 << art::mips64::T3) |
    (1 << art::mips64::S0) | (1 << art::mips64::S1) | (1 << art::mips64::T8) |
    (1 << art::mips64::T9);

static constexpr uint32_t kMips64CalleeSaveFpRefSpills = 0;
static constexpr uint32_t kMips64CalleeSaveFpArgSpills =
    (1 << art::mips64::F12) | (1 << art::mips64::F13) | (1 << art::mips64::F14) |
    (1 << art::mips64::F15) | (1 << art::mips64::F16) | (1 << art::mips64::F17) |
    (1 << art::mips64::F18) | (1 << art::mips64::F19);
// F12 should not be necessary to spill, as A0 is always in use.
static constexpr uint32_t kMips64CalleeSaveFpAllSpills =
    (1 << art::mips64::F24) | (1 << art::mips64::F25) | (1 << art::mips64::F26) |
    (1 << art::mips64::F27) | (1 << art::mips64::F28) | (1 << art::mips64::F29) |
    (1 << art::mips64::F30) | (1 << art::mips64::F31);
static constexpr uint32_t kMips64CalleeSaveFpEverythingSpills =
    (1 << art::mips64::F0) | (1 << art::mips64::F1) | (1 << art::mips64::F2) |
    (1 << art::mips64::F3) | (1 << art::mips64::F4) | (1 << art::mips64::F5) |
    (1 << art::mips64::F6) | (1 << art::mips64::F7) | (1 << art::mips64::F8) |
    (1 << art::mips64::F9) | (1 << art::mips64::F10) | (1 << art::mips64::F11) |
    (1 << art::mips64::F12) | (1 << art::mips64::F13) | (1 << art::mips64::F14) |
    (1 << art::mips64::F15) | (1 << art::mips64::F16) | (1 << art::mips64::F17) |
    (1 << art::mips64::F18) | (1 << art::mips64::F19) | (1 << art::mips64::F20) |
    (1 << art::mips64::F21) | (1 << art::mips64::F22) | (1 << art::mips64::F23) |
    (1 << art::mips64::F24) | (1 << art::mips64::F25) | (1 << art::mips64::F26) |
    (1 << art::mips64::F27) | (1 << art::mips64::F28) | (1 << art::mips64::F29) |
    (1 << art::mips64::F30) | (1 << art::mips64::F31);

constexpr uint32_t Mips64CalleeSaveCoreSpills(Runtime::CalleeSaveType type) {
  return kMips64CalleeSaveAlwaysSpills | kMips64CalleeSaveRefSpills |
      (type == Runtime::kSaveRefsAndArgs ? kMips64CalleeSaveArgSpills : 0) |
      (type == Runtime::kSaveAllCalleeSaves ? kMips64CalleeSaveAllSpills : 0) |
      (type == Runtime::kSaveEverything ? kMips64CalleeSaveEverythingSpills : 0);
}

constexpr uint32_t Mips64CalleeSaveFpSpills(Runtime::CalleeSaveType type) {
  return kMips64CalleeSaveFpRefSpills |
      (type == Runtime::kSaveRefsAndArgs ? kMips64CalleeSaveFpArgSpills : 0) |
      (type == Runtime::kSaveAllCalleeSaves ? kMips64CalleeSaveFpAllSpills : 0) |
      (type == Runtime::kSaveEverything ? kMips64CalleeSaveFpEverythingSpills : 0);
}

constexpr uint32_t Mips64CalleeSaveFrameSize(Runtime::CalleeSaveType type) {
  return RoundUp((POPCOUNT(Mips64CalleeSaveCoreSpills(type)) /* gprs */ +
                  POPCOUNT(Mips64CalleeSaveFpSpills(type))   /* fprs */ +
                  + 1 /* Method* */) * static_cast<size_t>(kMips64PointerSize), kStackAlignment);
}

constexpr QuickMethodFrameInfo Mips64CalleeSaveMethodFrameInfo(Runtime::CalleeSaveType type) {
  return QuickMethodFrameInfo(Mips64CalleeSaveFrameSize(type),
                              Mips64CalleeSaveCoreSpills(type),
                              Mips64CalleeSaveFpSpills(type));
}

}  // namespace mips64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS64_QUICK_METHOD_FRAME_INFO_MIPS64_H_
