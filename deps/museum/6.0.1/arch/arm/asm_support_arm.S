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

#ifndef ART_RUNTIME_ARCH_ARM_ASM_SUPPORT_ARM_S_
#define ART_RUNTIME_ARCH_ARM_ASM_SUPPORT_ARM_S_

#include "asm_support_arm.h"

// Define special registers.

// Register holding suspend check count down.
#define rSUSPEND r4
// Register holding Thread::Current().
#define rSELF r9

.syntax unified
.arch armv7-a
.thumb

// Macro to generate the value of Runtime::Current into rDest clobbering rTemp. As it uses labels
// then the labels need to be unique. We bind these to the function name in the ENTRY macros.
.macro RUNTIME_CURRENT name, num, rDest, rTemp
    .if .Lruntime_current\num\()_used
         .error
    .endif
    .set .Lruntime_current\num\()_used, 1
    ldr \rDest, .Lgot_\name\()_\num               @ Load offset of the GOT.
    ldr \rTemp, .Lruntime_instance_\name\()_\num  @ Load GOT offset of Runtime::instance_.
.Lload_got_\name\()_\num\():
    add \rDest, pc                                @ Fixup GOT address.
    ldr \rDest, [\rDest, \rTemp]                  @ Load address of Runtime::instance_.
    ldr \rDest, [\rDest]                          @ Load Runtime::instance_.
.endm

// Common ENTRY declaration code for ARM and thumb, an ENTRY should always be paired with an END.
// Declares the RUNTIME_CURRENT[123] macros that can be used within an ENTRY and will have literals
// generated at END.
.macro DEF_ENTRY thumb_or_arm, name
    \thumb_or_arm
    .type \name, #function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    // Cache alignment for function entry.
    .balign 16
\name:
    .cfi_startproc
    .fnstart
    // Track whether RUNTIME_CURRENT was used.
    .set .Lruntime_current1_used, 0
    .set .Lruntime_current2_used, 0
    .set .Lruntime_current3_used, 0
    // The RUNTIME_CURRENT macros that are bound to the \name argument of DEF_ENTRY to ensure
    // that label names are unique.
    .macro RUNTIME_CURRENT1 rDest, rTemp
        RUNTIME_CURRENT \name, 1, \rDest, \rTemp
    .endm
    .macro RUNTIME_CURRENT2 rDest, rTemp
        RUNTIME_CURRENT \name, 2, \rDest, \rTemp
    .endm
    .macro RUNTIME_CURRENT3 rDest, rTemp
        RUNTIME_CURRENT \name, 3, \rDest, \rTemp
    .endm
.endm

// A thumb2 style ENTRY.
.macro ENTRY name
    DEF_ENTRY .thumb_func, \name
.endm

// A ARM style ENTRY.
.macro ARM_ENTRY name
    DEF_ENTRY .arm, \name
.endm

// Terminate an ENTRY and generate GOT references.
.macro END name
     // Generate offsets of GOT and Runtime::instance_ used in RUNTIME_CURRENT.
     .if .Lruntime_current1_used
         .Lgot_\name\()_1:
             .word   _GLOBAL_OFFSET_TABLE_-(.Lload_got_\name\()_1+4)
         .Lruntime_instance_\name\()_1:
             .word   _ZN3art7Runtime9instance_E(GOT)
     .endif
     .if .Lruntime_current2_used
         .Lgot_\name\()_2:
             .word   _GLOBAL_OFFSET_TABLE_-(.Lload_got_\name\()_2+4)
         .Lruntime_instance_\name\()_2:
             .word   _ZN3art7Runtime9instance_E(GOT)
    .endif
     .if .Lruntime_current3_used
         .Lgot_\name\()_3:
             .word   _GLOBAL_OFFSET_TABLE_-(.Lload_got_\name\()_3+4)
         .Lruntime_instance_\name\()_3:
             .word   _ZN3art7Runtime9instance_E(GOT)
    .endif
    // Remove the RUNTIME_CURRENTx macros so they get rebound in the next function entry.
    .purgem RUNTIME_CURRENT1
    .purgem RUNTIME_CURRENT2
    .purgem RUNTIME_CURRENT3
    .fnend
    .cfi_endproc
    .size \name, .-\name
.endm

// Declare an unimplemented ENTRY that will halt a debugger.
.macro UNIMPLEMENTED name
    ENTRY \name
    bkpt
    bkpt
    END \name
.endm

#endif  // ART_RUNTIME_ARCH_X86_ASM_SUPPORT_X86_S_
