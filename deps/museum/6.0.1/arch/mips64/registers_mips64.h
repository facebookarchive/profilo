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

#ifndef ART_RUNTIME_ARCH_MIPS64_REGISTERS_MIPS64_H_
#define ART_RUNTIME_ARCH_MIPS64_REGISTERS_MIPS64_H_

#include <iosfwd>

#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {
namespace mips64 {

enum GpuRegister {
  ZERO =  0,
  AT   =  1,  // Assembler temporary.
  V0   =  2,  // Values.
  V1   =  3,
  A0   =  4,  // Arguments.
  A1   =  5,
  A2   =  6,
  A3   =  7,
  A4   =  8,
  A5   =  9,
  A6   = 10,
  A7   = 11,
  T0   = 12,  // Temporaries.
  T1   = 13,
  T2   = 14,
  T3   = 15,
  S0   = 16,  // Saved values.
  S1   = 17,
  S2   = 18,
  S3   = 19,
  S4   = 20,
  S5   = 21,
  S6   = 22,
  S7   = 23,
  T8   = 24,  // More temporaries.
  T9   = 25,
  K0   = 26,  // Reserved for trap handler.
  K1   = 27,
  GP   = 28,  // Global pointer.
  SP   = 29,  // Stack pointer.
  S8   = 30,  // Saved value/frame pointer.
  RA   = 31,  // Return address.
  kNumberOfGpuRegisters = 32,
  kNoGpuRegister = -1  // Signals an illegal register.
};
std::ostream& operator<<(std::ostream& os, const GpuRegister& rhs);

// Values for floating point registers.
enum FpuRegister {
  F0  =  0,
  F1  =  1,
  F2  =  2,
  F3  =  3,
  F4  =  4,
  F5  =  5,
  F6  =  6,
  F7  =  7,
  F8  =  8,
  F9  =  9,
  F10 = 10,
  F11 = 11,
  F12 = 12,
  F13 = 13,
  F14 = 14,
  F15 = 15,
  F16 = 16,
  F17 = 17,
  F18 = 18,
  F19 = 19,
  F20 = 20,
  F21 = 21,
  F22 = 22,
  F23 = 23,
  F24 = 24,
  F25 = 25,
  F26 = 26,
  F27 = 27,
  F28 = 28,
  F29 = 29,
  F30 = 30,
  F31 = 31,
  kNumberOfFpuRegisters = 32,
  kNoFpuRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const FpuRegister& rhs);

}  // namespace mips64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS64_REGISTERS_MIPS64_H_
