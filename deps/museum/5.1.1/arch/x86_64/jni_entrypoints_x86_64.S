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

#include "asm_support_x86_64.S"

    /*
     * Jni dlsym lookup stub.
     */
DEFINE_FUNCTION art_jni_dlsym_lookup_stub
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r9   // Arg.
    PUSH r8   // Arg.
    PUSH rdi  // JniEnv.
    PUSH rsi  // Arg.
    PUSH rdx  // Arg.
    PUSH rcx  // Arg.
    // Create space for FPR args, plus padding for alignment
    subq LITERAL(72 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(72 + 4 * 8)
    // Save FPRs.
    movq %xmm0, 0(%rsp)
    movq %xmm1, 8(%rsp)
    movq %xmm2, 16(%rsp)
    movq %xmm3, 24(%rsp)
    movq %xmm4, 32(%rsp)
    movq %xmm5, 40(%rsp)
    movq %xmm6, 48(%rsp)
    movq %xmm7, 56(%rsp)
    movq %xmm12, 64(%rsp)
    movq %xmm13, 72(%rsp)
    movq %xmm14, 80(%rsp)
    movq %xmm15, 88(%rsp)
    // prepare call
    movq %gs:THREAD_SELF_OFFSET, %rdi      // RDI := Thread::Current()
    // call
    call PLT_SYMBOL(artFindNativeMethod)  // (Thread*)
    // restore arguments
    movq 0(%rsp), %xmm0
    movq 8(%rsp), %xmm1
    movq 16(%rsp), %xmm2
    movq 24(%rsp), %xmm3
    movq 32(%rsp), %xmm4
    movq 40(%rsp), %xmm5
    movq 48(%rsp), %xmm6
    movq 56(%rsp), %xmm7
    movq 64(%rsp), %xmm12
    movq 72(%rsp), %xmm13
    movq 80(%rsp), %xmm14
    movq 88(%rsp), %xmm15
    addq LITERAL(72 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-72 - 4 * 8)
    POP rcx  // Arg.
    POP rdx  // Arg.
    POP rsi  // Arg.
    POP rdi  // JniEnv.
    POP r8   // Arg.
    POP r9   // Arg.
    testq %rax, %rax         // check if returned method code is null
    jz .Lno_native_code_found     // if null, jump to return to handle
    jmp *%rax                     // otherwise, tail call to intended method
.Lno_native_code_found:
    ret
END_FUNCTION art_jni_dlsym_lookup_stub
