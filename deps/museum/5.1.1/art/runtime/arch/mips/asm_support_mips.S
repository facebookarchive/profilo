/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_MIPS_ASM_SUPPORT_MIPS_S_
#define ART_RUNTIME_ARCH_MIPS_ASM_SUPPORT_MIPS_S_

#include "asm_support_mips.h"

// Define special registers.

// Register holding suspend check count down.
#define rSUSPEND $s0
// Register holding Thread::Current().
#define rSELF $s1


    /* Cache alignment for function entry */
.macro ENTRY name
    .type \name, %function
    .global \name
    .balign 16
\name:
    .cfi_startproc
     /* Ensure we get a sane starting CFA. */
    .cfi_def_cfa $sp,0
.endm

.macro END name
    .cfi_endproc
    .size \name, .-\name
.endm

    /* Generates $gp for function calls */
.macro GENERATE_GLOBAL_POINTER
    .cpload $t9
.endm

.macro UNIMPLEMENTED name
    ENTRY \name
    break
    break
    END \name
.endm


#endif  // ART_RUNTIME_ARCH_MIPS_ASM_SUPPORT_MIPS_S_
