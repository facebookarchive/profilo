/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "asm_support_mips.S"

    .set noreorder
    .balign 4

    /*
     * Jni dlsym lookup stub.
     */
    .extern artFindNativeMethod
ENTRY art_jni_dlsym_lookup_stub
    GENERATE_GLOBAL_POINTER
    addiu $sp, $sp, -32          # leave room for $a0, $a1, $a2, $a3, and $ra
    .cfi_adjust_cfa_offset 32
    sw     $ra, 16($sp)
    .cfi_rel_offset 31, 16
    sw     $a3, 12($sp)
    .cfi_rel_offset 7, 12
    sw     $a2, 8($sp)
    .cfi_rel_offset 6, 8
    sw     $a1, 4($sp)
    .cfi_rel_offset 5, 4
    sw     $a0, 0($sp)
    .cfi_rel_offset 4, 0
    jal   artFindNativeMethod   # (Thread*)
    move  $a0, $s1              # pass Thread::Current()
    lw    $a0, 0($sp)           # restore registers from stack
    lw    $a1, 4($sp)
    lw    $a2, 8($sp)
    lw    $a3, 12($sp)
    lw    $ra, 16($sp)
    beq   $v0, $zero, .Lno_native_code_found
    addiu $sp, $sp, 32          # restore the stack
    .cfi_adjust_cfa_offset -32
    move  $t9, $v0              # put method code result in $t9
    jr    $t9                   # leaf call to method's code
    nop
.Lno_native_code_found:
    jr    $ra
    nop
END art_jni_dlsym_lookup_stub
