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

#include "asm_support_x86.S"

#define MEMCMP  __memcmp16

/* int32_t memcmp16_compare(const uint16_t* s0, const uint16_t* s1, size_t count); */

#ifndef L
# define L(label)    .L##label
#endif

#define CFI_PUSH(REG)    \
    CFI_ADJUST_CFA_OFFSET(4);    \
    CFI_REL_OFFSET(REG, 0)

#define CFI_POP(REG)    \
    CFI_ADJUST_CFA_OFFSET(-4);    \
    CFI_RESTORE(REG)

#define PUSH(REG)    pushl REG; CFI_PUSH (REG)
#define POP(REG)    popl REG; CFI_POP (REG)

#define PARMS        4
#define BLK1        PARMS
#define BLK2        BLK1+4
#define LEN        BLK2+4
#define RETURN_END    POP (%edi); POP (%esi); POP (%ebx); ret
#define RETURN        RETURN_END; CFI_RESTORE_STATE; CFI_REMEMBER_STATE

DEFINE_FUNCTION MEMCMP
    movl       LEN(%esp), %ecx

    shl        $1, %ecx
    jz         L(zero)

    movl       BLK1(%esp), %eax
    cmp        $48, %ecx
    movl       BLK2(%esp), %edx
    jae        L(48bytesormore)

    PUSH       (%ebx)
    add        %ecx, %edx
    add        %ecx, %eax
    jmp        L(less48bytes)

    CFI_POP    (%ebx)

    .p2align 4
L(zero):
    xor        %eax, %eax
    ret

    .p2align 4
L(48bytesormore):
    PUSH       (%ebx)
    PUSH       (%esi)
    PUSH       (%edi)
    CFI_REMEMBER_STATE
    movdqu     (%eax), %xmm3
    movdqu     (%edx), %xmm0
    movl       %eax, %edi
    movl       %edx, %esi
    pcmpeqb    %xmm0, %xmm3
    pmovmskb   %xmm3, %edx
    lea        16(%edi), %edi

    sub        $0xffff, %edx
    lea        16(%esi), %esi
    jnz        L(less16bytes)
    mov        %edi, %edx
    and        $0xf, %edx
    xor        %edx, %edi
    sub        %edx, %esi
    add        %edx, %ecx
    mov        %esi, %edx
    and        $0xf, %edx
    jz         L(shr_0)
    xor        %edx, %esi

    cmp        $0, %edx
    je         L(shr_0)
    cmp        $2, %edx
    je         L(shr_2)
    cmp        $4, %edx
    je         L(shr_4)
    cmp        $6, %edx
    je         L(shr_6)
    cmp        $8, %edx
    je         L(shr_8)
    cmp        $10, %edx
    je         L(shr_10)
    cmp        $12, %edx
    je         L(shr_12)
    jmp        L(shr_14)

    .p2align 4
L(shr_0):
    cmp        $80, %ecx
    jae        L(shr_0_gobble)
    lea        -48(%ecx), %ecx
    xor        %eax, %eax
    movaps     (%esi), %xmm1
    pcmpeqb    (%edi), %xmm1
    movaps     16(%esi), %xmm2
    pcmpeqb    16(%edi), %xmm2
    pand       %xmm1, %xmm2
    pmovmskb   %xmm2, %edx
    add        $32, %edi
    add        $32, %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        (%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_0_gobble):
    lea        -48(%ecx), %ecx
    movdqa     (%esi), %xmm0
    xor        %eax, %eax
    pcmpeqb    (%edi), %xmm0
    sub        $32, %ecx
    movdqa     16(%esi), %xmm2
    pcmpeqb    16(%edi), %xmm2
L(shr_0_gobble_loop):
    pand       %xmm0, %xmm2
    sub        $32, %ecx
    pmovmskb   %xmm2, %edx
    movdqa     %xmm0, %xmm1
    movdqa     32(%esi), %xmm0
    movdqa     48(%esi), %xmm2
    sbb        $0xffff, %edx
    pcmpeqb    32(%edi), %xmm0
    pcmpeqb    48(%edi), %xmm2
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    jz         L(shr_0_gobble_loop)

    pand       %xmm0, %xmm2
    cmp        $0, %ecx
    jge        L(shr_0_gobble_loop_next)
    inc        %edx
    add        $32, %ecx
L(shr_0_gobble_loop_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb %xmm2, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        (%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_2):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_2_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $2,(%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $2,%xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        2(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_2_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $2,(%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $2,16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_2_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $2,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $2,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_2_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_2_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_2_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        2(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_4):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_4_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $4,(%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $4,%xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        4(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_4_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $4,(%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $4,16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_4_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $4,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $4,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_4_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_4_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_4_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        4(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_6):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_6_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $6,(%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $6,%xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        6(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_6_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $6,(%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $6,16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_6_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $6,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $6,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_6_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_6_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_6_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        6(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_8):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_8_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $8,(%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $8,%xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        8(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_8_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $8,(%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $8,16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_8_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $8,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $8,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_8_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_8_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_8_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        8(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_10):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_10_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $10, (%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $10,%xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        10(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_10_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $10, (%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $10, 16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_10_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $10,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $10,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_10_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_10_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_10_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        10(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_12):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_12_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $12, (%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $12, %xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        12(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_12_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $12, (%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $12, 16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_12_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $12,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $12,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_12_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_12_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_12_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        12(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_14):
    cmp        $80, %ecx
    lea        -48(%ecx), %ecx
    mov        %edx, %eax
    jae        L(shr_14_gobble)

    movdqa     16(%esi), %xmm1
    movdqa     %xmm1, %xmm2
    palignr    $14, (%esi), %xmm1
    pcmpeqb    (%edi), %xmm1

    movdqa     32(%esi), %xmm3
    palignr    $14, %xmm2, %xmm3
    pcmpeqb    16(%edi), %xmm3

    pand       %xmm1, %xmm3
    pmovmskb   %xmm3, %edx
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)
    lea        (%ecx, %edi,1), %eax
    lea        14(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(shr_14_gobble):
    sub        $32, %ecx
    movdqa     16(%esi), %xmm0
    palignr    $14, (%esi), %xmm0
    pcmpeqb    (%edi), %xmm0

    movdqa     32(%esi), %xmm3
    palignr    $14, 16(%esi), %xmm3
    pcmpeqb    16(%edi), %xmm3

L(shr_14_gobble_loop):
    pand       %xmm0, %xmm3
    sub        $32, %ecx
    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1

    movdqa     64(%esi), %xmm3
    palignr    $14,48(%esi), %xmm3
    sbb        $0xffff, %edx
    movdqa     48(%esi), %xmm0
    palignr    $14,32(%esi), %xmm0
    pcmpeqb    32(%edi), %xmm0
    lea        32(%esi), %esi
    pcmpeqb    48(%edi), %xmm3

    lea        32(%edi), %edi
    jz         L(shr_14_gobble_loop)
    pand       %xmm0, %xmm3

    cmp        $0, %ecx
    jge        L(shr_14_gobble_next)
    inc        %edx
    add        $32, %ecx
L(shr_14_gobble_next):
    test       %edx, %edx
    jnz        L(exit)

    pmovmskb   %xmm3, %edx
    movdqa     %xmm0, %xmm1
    lea        32(%edi), %edi
    lea        32(%esi), %esi
    sub        $0xffff, %edx
    jnz        L(exit)

    lea        (%ecx, %edi,1), %eax
    lea        14(%ecx, %esi,1), %edx
    POP        (%edi)
    POP        (%esi)
    jmp        L(less48bytes)

    CFI_RESTORE_STATE
    CFI_REMEMBER_STATE
    .p2align 4
L(exit):
    pmovmskb   %xmm1, %ebx
    sub        $0xffff, %ebx
    jz         L(first16bytes)
    lea        -16(%esi), %esi
    lea        -16(%edi), %edi
    mov        %ebx, %edx

L(first16bytes):
    add        %eax, %esi
L(less16bytes):
    test       %dl, %dl
    jz         L(next_four_words)
    test       $15, %dl
    jz         L(second_two_words)
    test       $3, %dl
    jz         L(second_word)
    movzwl     -16(%edi), %eax
    movzwl     -16(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(second_word):
    movzwl     -14(%edi), %eax
    movzwl     -14(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(second_two_words):
    test       $63, %dl
    jz         L(fourth_word)
    movzwl     -12(%edi), %eax
    movzwl     -12(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(fourth_word):
    movzwl     -10(%edi), %eax
    movzwl     -10(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(next_four_words):
    test       $15, %dh
    jz         L(fourth_two_words)
    test       $3, %dh
    jz         L(sixth_word)
    movzwl     -8(%edi), %eax
    movzwl     -8(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(sixth_word):
    movzwl     -6(%edi), %eax
    movzwl     -6(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(fourth_two_words):
    test       $63, %dh
    jz         L(eighth_word)
    movzwl     -4(%edi), %eax
    movzwl     -4(%esi), %ebx
    subl       %ebx, %eax
    RETURN

    .p2align 4
L(eighth_word):
    movzwl     -2(%edi), %eax
    movzwl     -2(%esi), %ebx
    subl       %ebx, %eax
    RETURN


    CFI_PUSH (%ebx)

    .p2align 4
L(more8bytes):
    cmp        $16, %ecx
    jae        L(more16bytes)
    cmp        $8, %ecx
    je         L(8bytes)
    cmp        $10, %ecx
    je         L(10bytes)
    cmp        $12, %ecx
    je         L(12bytes)
    jmp        L(14bytes)

    .p2align 4
L(more16bytes):
    cmp        $24, %ecx
    jae        L(more24bytes)
    cmp        $16, %ecx
    je         L(16bytes)
    cmp        $18, %ecx
    je         L(18bytes)
    cmp        $20, %ecx
    je         L(20bytes)
    jmp        L(22bytes)

    .p2align 4
L(more24bytes):
    cmp        $32, %ecx
    jae        L(more32bytes)
    cmp        $24, %ecx
    je         L(24bytes)
    cmp        $26, %ecx
    je         L(26bytes)
    cmp        $28, %ecx
    je         L(28bytes)
    jmp        L(30bytes)

    .p2align 4
L(more32bytes):
    cmp        $40, %ecx
    jae        L(more40bytes)
    cmp        $32, %ecx
    je         L(32bytes)
    cmp        $34, %ecx
    je         L(34bytes)
    cmp        $36, %ecx
    je         L(36bytes)
    jmp        L(38bytes)

    .p2align 4
L(less48bytes):
    cmp        $8, %ecx
    jae        L(more8bytes)
    cmp        $2, %ecx
    je         L(2bytes)
    cmp        $4, %ecx
    je         L(4bytes)
    jmp        L(6bytes)

    .p2align 4
L(more40bytes):
    cmp        $40, %ecx
    je         L(40bytes)
    cmp        $42, %ecx
    je         L(42bytes)
    cmp        $44, %ecx
    je         L(44bytes)
    jmp        L(46bytes)

    .p2align 4
L(46bytes):
    movzwl     -46(%eax), %ecx
    movzwl     -46(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(44bytes):
    movzwl     -44(%eax), %ecx
    movzwl     -44(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(42bytes):
    movzwl     -42(%eax), %ecx
    movzwl     -42(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(40bytes):
    movzwl     -40(%eax), %ecx
    movzwl     -40(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(38bytes):
    movzwl     -38(%eax), %ecx
    movzwl     -38(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(36bytes):
    movzwl     -36(%eax), %ecx
    movzwl     -36(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(34bytes):
    movzwl     -34(%eax), %ecx
    movzwl     -34(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(32bytes):
    movzwl     -32(%eax), %ecx
    movzwl     -32(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(30bytes):
    movzwl     -30(%eax), %ecx
    movzwl     -30(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(28bytes):
    movzwl     -28(%eax), %ecx
    movzwl     -28(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(26bytes):
    movzwl     -26(%eax), %ecx
    movzwl     -26(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(24bytes):
    movzwl     -24(%eax), %ecx
    movzwl     -24(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(22bytes):
    movzwl     -22(%eax), %ecx
    movzwl     -22(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(20bytes):
    movzwl     -20(%eax), %ecx
    movzwl     -20(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(18bytes):
    movzwl     -18(%eax), %ecx
    movzwl     -18(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(16bytes):
    movzwl     -16(%eax), %ecx
    movzwl     -16(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(14bytes):
    movzwl     -14(%eax), %ecx
    movzwl     -14(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(12bytes):
    movzwl     -12(%eax), %ecx
    movzwl     -12(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(10bytes):
    movzwl     -10(%eax), %ecx
    movzwl     -10(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(8bytes):
    movzwl     -8(%eax), %ecx
    movzwl     -8(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(6bytes):
    movzwl     -6(%eax), %ecx
    movzwl     -6(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(4bytes):
    movzwl     -4(%eax), %ecx
    movzwl     -4(%edx), %ebx
    subl       %ebx, %ecx
    jne        L(memcmp16_exit)
L(2bytes):
    movzwl     -2(%eax), %eax
    movzwl     -2(%edx), %ebx
    subl       %ebx, %eax
    POP        (%ebx)
    ret
    CFI_PUSH   (%ebx)

    .p2align 4
L(memcmp16_exit):
    POP        (%ebx)
    mov        %ecx, %eax
    ret
END_FUNCTION MEMCMP
