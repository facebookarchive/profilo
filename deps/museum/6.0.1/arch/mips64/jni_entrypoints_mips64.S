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

#include "asm_support_mips64.S"

    .set noreorder
    .balign 16

    /*
     * Jni dlsym lookup stub.
     */
    .extern artFindNativeMethod
ENTRY art_jni_dlsym_lookup_stub
    daddiu $sp, $sp, -80        # save a0-a7 and $ra
    .cfi_adjust_cfa_offset 80
    sd     $ra, 64($sp)
    .cfi_rel_offset 31, 64
    sd     $a7, 56($sp)
    .cfi_rel_offset 11, 56
    sd     $a6, 48($sp)
    .cfi_rel_offset 10, 48
    sd     $a5, 40($sp)
    .cfi_rel_offset 9, 40
    sd     $a4, 32($sp)
    .cfi_rel_offset 8, 32
    sd     $a3, 24($sp)
    .cfi_rel_offset 7, 24
    sd     $a2, 16($sp)
    .cfi_rel_offset 6, 16
    sd     $a1, 8($sp)
    .cfi_rel_offset 5, 8
    sd     $a0, 0($sp)
    .cfi_rel_offset 4, 0
    move   $a0, $s1             # pass Thread::Current()
    jal    artFindNativeMethod  # (Thread*)
    .cpreturn                   # Restore gp from t8 in branch delay slot. gp is not used
                                # anymore, and t8 may be clobbered in artFindNativeMethod.

    ld     $a0, 0($sp)          # restore registers from stack
    .cfi_restore 4
    ld     $a1, 8($sp)
    .cfi_restore 5
    ld     $a2, 16($sp)
    .cfi_restore 6
    ld     $a3, 24($sp)
    .cfi_restore 7
    ld     $a4, 32($sp)
    .cfi_restore 8
    ld     $a5, 40($sp)
    .cfi_restore 9
    ld     $a6, 48($sp)
    .cfi_restore 10
    ld     $a7, 56($sp)
    .cfi_restore 11
    ld     $ra, 64($sp)
    .cfi_restore 31
    beq    $v0, $zero, .Lno_native_code_found
    daddiu $sp, $sp, 80         # restore the stack
    .cfi_adjust_cfa_offset -80
    move   $t9, $v0             # put method code result in $t9
    jalr   $zero, $t9           # leaf call to method's code
    nop
.Lno_native_code_found:
    jalr   $zero, $ra
    nop
END art_jni_dlsym_lookup_stub
