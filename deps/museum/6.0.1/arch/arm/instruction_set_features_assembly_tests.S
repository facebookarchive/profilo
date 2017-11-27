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

#include "asm_support_arm.S"

.section .text
// This function is used to check for the CPU's support for the sdiv
// instruction at runtime.  It will either return the value 1 or
// will cause an invalid instruction trap (SIGILL signal).  The
// caller must arrange for the signal handler to set the r0
// register to 0 and move the pc forward by 4 bytes (to skip
// the invalid instruction).
ENTRY artCheckForArmSdivInstruction
  mov r1,#1
  // depending on the architecture, the assembler will not allow an
  // sdiv instruction, so we will have to output the bytes directly.

  // sdiv r0,r1,r1 is two words: 0xfb91 0xf1f0.  We need little endian.
  .byte 0x91,0xfb,0xf1,0xf0

  // if the divide worked, r0 will have the value #1 (result of sdiv).
  // It will have 0 otherwise (set by the signal handler)
  // the value is just returned from this function.
  bx lr
END artCheckForArmSdivInstruction
