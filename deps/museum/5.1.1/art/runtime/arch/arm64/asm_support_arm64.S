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

#ifndef ART_RUNTIME_ARCH_ARM64_ASM_SUPPORT_ARM64_S_
#define ART_RUNTIME_ARCH_ARM64_ASM_SUPPORT_ARM64_S_

#include "asm_support_arm64.h"

// Define special registers.

// Register holding suspend check count down.
// 32-bit is enough for the suspend register.
#define wSUSPEND w19
// xSUSPEND is 64-bit view of wSUSPEND.
// Used to save/restore the register scratched by managed code.
#define xSUSPEND x19
// Register holding Thread::Current().
#define xSELF x18
// x18 is not preserved by aapcs64, save it on xETR(External Thread reg) for restore and later use.
#define xETR x21
// Frame Pointer
#define xFP   x29
// Link Register
#define xLR   x30
// Define the intraprocedural linkage temporary registers.
#define xIP0 x16
#define wIP0 w16
#define xIP1 x17
#define wIP1 w17


.macro ENTRY name
    .type \name, #function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    /* Cache alignment for function entry */
    .balign 16
\name:
    .cfi_startproc
.endm

.macro ENTRY_NO_HIDE name
    .type \name, #function
    .global \name
    /* Cache alignment for function entry */
    .balign 16
\name:
    .cfi_startproc
.endm

.macro END name
    .cfi_endproc
    .size \name, .-\name
.endm

.macro UNIMPLEMENTED name
    ENTRY \name
    brk 0
    END \name
.endm

.macro UNIMPLEMENTED_NO_HIDE name
    ENTRY_NO_HIDE \name
    brk 0
    END \name
.endm

#endif  // ART_RUNTIME_ARCH_ARM64_ASM_SUPPORT_ARM64_S_
