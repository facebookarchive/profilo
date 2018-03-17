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

MACRO0(SETUP_FP_CALLEE_SAVE_FRAME)
    // Create space for ART FP callee-saved registers
    subq MACRO_LITERAL(4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    movq %xmm12, 0(%rsp)
    movq %xmm13, 8(%rsp)
    movq %xmm14, 16(%rsp)
    movq %xmm15, 24(%rsp)
END_MACRO

MACRO0(RESTORE_FP_CALLEE_SAVE_FRAME)
    // Restore ART FP callee-saved registers
    movq 0(%rsp), %xmm12
    movq 8(%rsp), %xmm13
    movq 16(%rsp), %xmm14
    movq 24(%rsp), %xmm15
    addq MACRO_LITERAL(4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(- 4 * 8)
END_MACRO

// For x86, the CFA is esp+4, the address above the pushed return address on the stack.

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     */
MACRO0(SETUP_SAVE_ALL_CALLEE_SAVE_FRAME)
#if defined(__APPLE__)
    int3
    int3
#else
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save callee save registers to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    // Create space for FPR args, plus space for ArtMethod*.
    subq MACRO_LITERAL(4 * 8 + 8), %rsp
    CFI_ADJUST_CFA_OFFSET(4 * 8 + 8)
    // Save FPRs.
    movq %xmm12, 8(%rsp)
    movq %xmm13, 16(%rsp)
    movq %xmm14, 24(%rsp)
    movq %xmm15, 32(%rsp)
    // R10 := ArtMethod* for save all callee save frame method.
    THIS_LOAD_REQUIRES_READ_BARRIER
    movq RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET(%r10), %r10
    // Store ArtMethod* to bottom of stack.
    movq %r10, 0(%rsp)
    // Store rsp as the top quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVE != 6 * 8 + 4 * 8 + 8 + 8)
#error "SAVE_ALL_CALLEE_SAVE_FRAME(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly)
     */
MACRO0(SETUP_REFS_ONLY_CALLEE_SAVE_FRAME)
#if defined(__APPLE__)
    int3
    int3
#else
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    // Create space for FPR args, plus space for ArtMethod*.
    subq LITERAL(8 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(8 + 4 * 8)
    // Save FPRs.
    movq %xmm12, 8(%rsp)
    movq %xmm13, 16(%rsp)
    movq %xmm14, 24(%rsp)
    movq %xmm15, 32(%rsp)
    // R10 := ArtMethod* for refs only callee save frame method.
    THIS_LOAD_REQUIRES_READ_BARRIER
    movq RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET(%r10), %r10
    // Store ArtMethod* to bottom of stack.
    movq %r10, 0(%rsp)
    // Store rsp as the stop quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE != 6 * 8 + 4 * 8 + 8 + 8)
#error "REFS_ONLY_CALLEE_SAVE_FRAME(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

MACRO0(RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME)
    movq 8(%rsp), %xmm12
    movq 16(%rsp), %xmm13
    movq 24(%rsp), %xmm14
    movq 32(%rsp), %xmm15
    addq LITERAL(8 + 4*8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8 - 4*8)
    // TODO: optimize by not restoring callee-saves restored by the ABI
    POP rbx
    POP rbp
    POP r12
    POP r13
    POP r14
    POP r15
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs)
     */
MACRO0(SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME)
#if defined(__APPLE__)
    int3
    int3
#else
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH r9   // Quick arg 5.
    PUSH r8   // Quick arg 4.
    PUSH rsi  // Quick arg 1.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    PUSH rdx  // Quick arg 2.
    PUSH rcx  // Quick arg 3.
    // Create space for FPR args and create 2 slots for ArtMethod*.
    subq MACRO_LITERAL(80 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(80 + 4 * 8)
    // R10 := ArtMethod* for ref and args callee save frame method.
    THIS_LOAD_REQUIRES_READ_BARRIER
    movq RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET(%r10), %r10
    // Save FPRs.
    movq %xmm0, 16(%rsp)
    movq %xmm1, 24(%rsp)
    movq %xmm2, 32(%rsp)
    movq %xmm3, 40(%rsp)
    movq %xmm4, 48(%rsp)
    movq %xmm5, 56(%rsp)
    movq %xmm6, 64(%rsp)
    movq %xmm7, 72(%rsp)
    movq %xmm12, 80(%rsp)
    movq %xmm13, 88(%rsp)
    movq %xmm14, 96(%rsp)
    movq %xmm15, 104(%rsp)
    // Store ArtMethod* to bottom of stack.
    movq %r10, 0(%rsp)
    // Store rsp as the top quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE != 11 * 8 + 4 * 8 + 80 + 8)
#error "REFS_AND_ARGS_CALLEE_SAVE_FRAME(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

MACRO0(SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_RDI)
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH r9   // Quick arg 5.
    PUSH r8   // Quick arg 4.
    PUSH rsi  // Quick arg 1.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    PUSH rdx  // Quick arg 2.
    PUSH rcx  // Quick arg 3.
    // Create space for FPR args and create 2 slots for ArtMethod*.
    subq LITERAL(80 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(80 + 4 * 8)
    // Save FPRs.
    movq %xmm0, 16(%rsp)
    movq %xmm1, 24(%rsp)
    movq %xmm2, 32(%rsp)
    movq %xmm3, 40(%rsp)
    movq %xmm4, 48(%rsp)
    movq %xmm5, 56(%rsp)
    movq %xmm6, 64(%rsp)
    movq %xmm7, 72(%rsp)
    movq %xmm12, 80(%rsp)
    movq %xmm13, 88(%rsp)
    movq %xmm14, 96(%rsp)
    movq %xmm15, 104(%rsp)
    // Store ArtMethod to bottom of stack.
    movq %rdi, 0(%rsp)
    // Store rsp as the stop quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET
END_MACRO

MACRO0(RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME)
    // Restore FPRs.
    movq 16(%rsp), %xmm0
    movq 24(%rsp), %xmm1
    movq 32(%rsp), %xmm2
    movq 40(%rsp), %xmm3
    movq 48(%rsp), %xmm4
    movq 56(%rsp), %xmm5
    movq 64(%rsp), %xmm6
    movq 72(%rsp), %xmm7
    movq 80(%rsp), %xmm12
    movq 88(%rsp), %xmm13
    movq 96(%rsp), %xmm14
    movq 104(%rsp), %xmm15
    addq MACRO_LITERAL(80 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(80 + 4 * 8))
    // Restore callee and GPR args, mixed together to agree with core spills bitmap.
    POP rcx
    POP rdx
    POP rbx
    POP rbp
    POP rsi
    POP r8
    POP r9
    POP r12
    POP r13
    POP r14
    POP r15
END_MACRO


    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME         // save callee saves for throw
    // (Thread*) setup
    movq %gs:THREAD_SELF_OFFSET, %rdi
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*)
    UNREACHABLE
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdi  // pass Thread::Current()
    call VAR(cxx_name, 1)     // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO2(ONE_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rsi  // pass Thread::Current()
    call VAR(cxx_name, 1)     // cxx_name(arg1, Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO2(TWO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdx  // pass Thread::Current()
    call VAR(cxx_name, 1)     // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name, 0)
END_MACRO

    /*
     * Called by managed code to create and deliver a NullPointerException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_null_pointer_exception, artThrowNullPointerExceptionFromCode

    /*
     * Called by managed code to create and deliver an ArithmeticException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_div_zero, artThrowDivZeroFromCode

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_stack_overflow, artThrowStackOverflowFromCode

    /*
     * Called by managed code, saves callee saves and then calls artThrowException
     * that will place a mock Method* at the bottom of the stack. Arg1 holds the exception.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_deliver_exception, artDeliverExceptionFromCode

    /*
     * Called by managed code to create and deliver a NoSuchMethodError.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_throw_no_such_method, artThrowNoSuchMethodFromCode

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException. Arg1 holds
     * index, arg2 holds limit.
     */
TWO_ARG_RUNTIME_EXCEPTION art_quick_throw_array_bounds, artThrowArrayBoundsFromCode

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/rdi with the target Method*, arg0/rdi will contain
     * the method_idx.  This wrapper will save arg1-arg3, load the caller's Method*, align the
     * stack and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/rsi.
     *
     * The helper will attempt to locate the target and return a 128-bit result in rax/rdx consisting
     * of the target Method* in rax and method->code_ in rdx.
     *
     * If unsuccessful, the helper will return null/????. There will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the return
     * location on the stack.
     *
     * Adapted from x86 code.
     */
MACRO2(INVOKE_TRAMPOLINE, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME  // save callee saves in case allocation triggers GC
    // Helper signature is always
    // (method_idx, *this_object, *caller_method, *self, sp)

    movq FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE(%rsp), %rdx  // pass caller Method*
    movq %gs:THREAD_SELF_OFFSET, %rcx                      // pass Thread
    movq %rsp, %r8                                         // pass SP

    call VAR(cxx_name, 1)                   // cxx_name(arg1, arg2, caller method*, Thread*, SP)
                                                           // save the code pointer
    movq %rax, %rdi
    movq %rdx, %rax
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME

    testq %rdi, %rdi
    jz 1f

    // Tail call to intended method.
    jmp *%rax
1:
    DELIVER_PENDING_EXCEPTION
    END_FUNCTION VAR(c_name, 0)
END_MACRO

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline, artInvokeInterfaceTrampoline
INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck


    /*
     * Helper for quick invocation stub to set up XMM registers. Assumes r10 == shorty,
     * r11 == arg_array. Clobbers r10, r11 and al. Branches to xmm_setup_finished if it encounters
     * the end of the shorty.
     */
MACRO2(LOOP_OVER_SHORTY_LOADING_XMMS, xmm_reg, finished)
1: // LOOP
    movb (%r10), %al              // al := *shorty
    addq MACRO_LITERAL(1), %r10   // shorty++
    cmpb MACRO_LITERAL(0), %al    // if (al == '\0') goto xmm_setup_finished
    je VAR(finished, 1)
    cmpb MACRO_LITERAL(68), %al   // if (al == 'D') goto FOUND_DOUBLE
    je 2f
    cmpb MACRO_LITERAL(70), %al   // if (al == 'F') goto FOUND_FLOAT
    je 3f
    addq MACRO_LITERAL(4), %r11   // arg_array++
    //  Handle extra space in arg array taken by a long.
    cmpb MACRO_LITERAL(74), %al   // if (al != 'J') goto LOOP
    jne 1b
    addq MACRO_LITERAL(4), %r11   // arg_array++
    jmp 1b                        // goto LOOP
2:  // FOUND_DOUBLE
    movsd (%r11), REG_VAR(xmm_reg, 0)
    addq MACRO_LITERAL(8), %r11   // arg_array+=2
    jmp 4f
3:  // FOUND_FLOAT
    movss (%r11), REG_VAR(xmm_reg, 0)
    addq MACRO_LITERAL(4), %r11   // arg_array++
4:
END_MACRO

    /*
     * Helper for quick invocation stub to set up GPR registers. Assumes r10 == shorty,
     * r11 == arg_array. Clobbers r10, r11 and al. Branches to gpr_setup_finished if it encounters
     * the end of the shorty.
     */
MACRO3(LOOP_OVER_SHORTY_LOADING_GPRS, gpr_reg64, gpr_reg32, finished)
1: // LOOP
    movb (%r10), %al              // al := *shorty
    addq MACRO_LITERAL(1), %r10   // shorty++
    cmpb MACRO_LITERAL(0), %al    // if (al == '\0') goto gpr_setup_finished
    je  VAR(finished, 2)
    cmpb MACRO_LITERAL(74), %al   // if (al == 'J') goto FOUND_LONG
    je 2f
    cmpb MACRO_LITERAL(70), %al   // if (al == 'F') goto SKIP_FLOAT
    je 3f
    cmpb MACRO_LITERAL(68), %al   // if (al == 'D') goto SKIP_DOUBLE
    je 4f
    movl (%r11), REG_VAR(gpr_reg32, 1)
    addq MACRO_LITERAL(4), %r11   // arg_array++
    jmp 5f
2:  // FOUND_LONG
    movq (%r11), REG_VAR(gpr_reg64, 0)
    addq MACRO_LITERAL(8), %r11   // arg_array+=2
    jmp 5f
3:  // SKIP_FLOAT
    addq MACRO_LITERAL(4), %r11   // arg_array++
    jmp 1b
4:  // SKIP_DOUBLE
    addq MACRO_LITERAL(8), %r11   // arg_array+=2
    jmp 1b
5:
END_MACRO

    /*
     * Quick invocation stub.
     * On entry:
     *   [sp] = return address
     *   rdi = method pointer
     *   rsi = argument array that must at least contain the this pointer.
     *   rdx = size of argument array in bytes
     *   rcx = (managed) thread pointer
     *   r8 = JValue* result
     *   r9 = char* shorty
     */
DEFINE_FUNCTION art_quick_invoke_stub
#if defined(__APPLE__)
    int3
    int3
#else
    // Set up argument XMM registers.
    leaq 1(%r9), %r10             // R10 := shorty + 1  ; ie skip return arg character.
    leaq 4(%rsi), %r11            // R11 := arg_array + 4 ; ie skip this pointer.
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm4, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm5, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm6, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm7, .Lxmm_setup_finished
    .balign 16
.Lxmm_setup_finished:
    PUSH rbp                      // Save rbp.
    PUSH r8                       // Save r8/result*.
    PUSH r9                       // Save r9/shorty*.
    PUSH rbx                      // Save native callee save rbx
    PUSH r12                      // Save native callee save r12
    PUSH r13                      // Save native callee save r13
    PUSH r14                      // Save native callee save r14
    PUSH r15                      // Save native callee save r15
    movq %rsp, %rbp               // Copy value of stack pointer into base pointer.
    CFI_DEF_CFA_REGISTER(rbp)

    movl %edx, %r10d
    addl LITERAL(100), %edx        // Reserve space for return addr, StackReference<method>, rbp,
                                   // r8, r9, rbx, r12, r13, r14, and r15 in frame.
    andl LITERAL(0xFFFFFFF0), %edx // Align frame size to 16 bytes.
    subl LITERAL(72), %edx         // Remove space for return address, rbp, r8, r9, rbx, r12,
                                   // r13, r14, and r15
    subq %rdx, %rsp                // Reserve stack space for argument array.

#if (STACK_REFERENCE_SIZE != 4)
#error "STACK_REFERENCE_SIZE(X86_64) size not as expected."
#endif
    movq LITERAL(0), (%rsp)       // Store null for method*

    movl %r10d, %ecx              // Place size of args in rcx.
    movq %rdi, %rax               // rax := method to be called
    movq %rsi, %r11               // r11 := arg_array
    leaq 8(%rsp), %rdi            // rdi is pointing just above the ArtMethod* in the stack
                                  // arguments.
    // Copy arg array into stack.
    rep movsb                     // while (rcx--) { *rdi++ = *rsi++ }
    leaq 1(%r9), %r10             // r10 := shorty + 1  ; ie skip return arg character
    movq %rax, %rdi               // rdi := method to be called
    movl (%r11), %esi             // rsi := this pointer
    addq LITERAL(4), %r11         // arg_array++
    LOOP_OVER_SHORTY_LOADING_GPRS rdx, edx, .Lgpr_setup_finished
    LOOP_OVER_SHORTY_LOADING_GPRS rcx, ecx, .Lgpr_setup_finished
    LOOP_OVER_SHORTY_LOADING_GPRS r8, r8d, .Lgpr_setup_finished
    LOOP_OVER_SHORTY_LOADING_GPRS r9, r9d, .Lgpr_setup_finished
.Lgpr_setup_finished:
    call *ART_METHOD_QUICK_CODE_OFFSET_64(%rdi) // Call the method.
    movq %rbp, %rsp               // Restore stack pointer.
    POP r15                       // Pop r15
    POP r14                       // Pop r14
    POP r13                       // Pop r13
    POP r12                       // Pop r12
    POP rbx                       // Pop rbx
    POP r9                        // Pop r9 - shorty*
    POP r8                        // Pop r8 - result*.
    POP rbp                       // Pop rbp
    cmpb LITERAL(68), (%r9)       // Test if result type char == 'D'.
    je .Lreturn_double_quick
    cmpb LITERAL(70), (%r9)       // Test if result type char == 'F'.
    je .Lreturn_float_quick
    movq %rax, (%r8)              // Store the result assuming its a long, int or Object*
    ret
.Lreturn_double_quick:
    movsd %xmm0, (%r8)            // Store the double floating point result.
    ret
.Lreturn_float_quick:
    movss %xmm0, (%r8)            // Store the floating point result.
    ret
#endif  // __APPLE__
END_FUNCTION art_quick_invoke_stub

    /*
     * Quick invocation stub.
     * On entry:
     *   [sp] = return address
     *   rdi = method pointer
     *   rsi = argument array or null if no arguments.
     *   rdx = size of argument array in bytes
     *   rcx = (managed) thread pointer
     *   r8 = JValue* result
     *   r9 = char* shorty
     */
DEFINE_FUNCTION art_quick_invoke_static_stub
#if defined(__APPLE__)
    int3
    int3
#else
    // Set up argument XMM registers.
    leaq 1(%r9), %r10             // R10 := shorty + 1  ; ie skip return arg character
    movq %rsi, %r11               // R11 := arg_array
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm4, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm5, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm6, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm7, .Lxmm_setup_finished2
    .balign 16
.Lxmm_setup_finished2:
    PUSH rbp                      // Save rbp.
    PUSH r8                       // Save r8/result*.
    PUSH r9                       // Save r9/shorty*.
    PUSH rbx                      // Save rbx
    PUSH r12                      // Save r12
    PUSH r13                      // Save r13
    PUSH r14                      // Save r14
    PUSH r15                      // Save r15
    movq %rsp, %rbp               // Copy value of stack pointer into base pointer.
    CFI_DEF_CFA_REGISTER(rbp)

    movl %edx, %r10d
    addl LITERAL(100), %edx        // Reserve space for return addr, StackReference<method>, rbp,
                                   // r8, r9, r12, r13, r14, and r15 in frame.
    andl LITERAL(0xFFFFFFF0), %edx // Align frame size to 16 bytes.
    subl LITERAL(72), %edx         // Remove space for return address, rbp, r8, r9, rbx, r12,
                                   // r13, r14, and r15.
    subq %rdx, %rsp                // Reserve stack space for argument array.

#if (STACK_REFERENCE_SIZE != 4)
#error "STACK_REFERENCE_SIZE(X86_64) size not as expected."
#endif
    movq LITERAL(0), (%rsp)        // Store null for method*

    movl %r10d, %ecx               // Place size of args in rcx.
    movq %rdi, %rax                // rax := method to be called
    movq %rsi, %r11                // r11 := arg_array
    leaq 8(%rsp), %rdi             // rdi is pointing just above the ArtMethod* in the
                                   // stack arguments.
    // Copy arg array into stack.
    rep movsb                      // while (rcx--) { *rdi++ = *rsi++ }
    leaq 1(%r9), %r10              // r10 := shorty + 1  ; ie skip return arg character
    movq %rax, %rdi                // rdi := method to be called
    LOOP_OVER_SHORTY_LOADING_GPRS rsi, esi, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS rdx, edx, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS rcx, ecx, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS r8, r8d, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS r9, r9d, .Lgpr_setup_finished2
.Lgpr_setup_finished2:
    call *ART_METHOD_QUICK_CODE_OFFSET_64(%rdi) // Call the method.
    movq %rbp, %rsp                // Restore stack pointer.
    POP r15                        // Pop r15
    POP r14                        // Pop r14
    POP r13                        // Pop r13
    POP r12                        // Pop r12
    POP rbx                        // Pop rbx
    POP r9                         // Pop r9 - shorty*.
    POP r8                         // Pop r8 - result*.
    POP rbp                        // Pop rbp
    cmpb LITERAL(68), (%r9)        // Test if result type char == 'D'.
    je .Lreturn_double_quick2
    cmpb LITERAL(70), (%r9)        // Test if result type char == 'F'.
    je .Lreturn_float_quick2
    movq %rax, (%r8)               // Store the result assuming its a long, int or Object*
    ret
.Lreturn_double_quick2:
    movsd %xmm0, (%r8)             // Store the double floating point result.
    ret
.Lreturn_float_quick2:
    movss %xmm0, (%r8)             // Store the floating point result.
    ret
#endif  // __APPLE__
END_FUNCTION art_quick_invoke_static_stub

    /*
     * Long jump stub.
     * On entry:
     *   rdi = gprs
     *   rsi = fprs
     */
DEFINE_FUNCTION art_quick_do_long_jump
#if defined(__APPLE__)
    int3
    int3
#else
    // Restore FPRs.
    movq 0(%rsi), %xmm0
    movq 8(%rsi), %xmm1
    movq 16(%rsi), %xmm2
    movq 24(%rsi), %xmm3
    movq 32(%rsi), %xmm4
    movq 40(%rsi), %xmm5
    movq 48(%rsi), %xmm6
    movq 56(%rsi), %xmm7
    movq 64(%rsi), %xmm8
    movq 72(%rsi), %xmm9
    movq 80(%rsi), %xmm10
    movq 88(%rsi), %xmm11
    movq 96(%rsi), %xmm12
    movq 104(%rsi), %xmm13
    movq 112(%rsi), %xmm14
    movq 120(%rsi), %xmm15
    // Restore FPRs.
    movq %rdi, %rsp   // RSP points to gprs.
    // Load all registers except RSP and RIP with values in gprs.
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    addq LITERAL(8), %rsp   // Skip rsp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    popq %rsp      // Load stack pointer.
    ret            // From higher in the stack pop rip.
#endif  // __APPLE__
END_FUNCTION art_quick_do_long_jump

MACRO3(NO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdi    // pass Thread::Current()
    call VAR(cxx_name, 1)                // cxx_name(Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)          // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(ONE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rsi    // pass Thread::Current()
    call VAR(cxx_name, 1)                // cxx_name(arg0, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)          // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(TWO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdx    // pass Thread::Current()
    call VAR(cxx_name, 1)                // cxx_name(arg0, arg1, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)          // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(THREE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME   // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rcx   // pass Thread::Current()
    call VAR(cxx_name, 1)               // cxx_name(arg0, arg1, arg2, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME // restore frame up to return address
    CALL_MACRO(return_macro, 2)         // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(FOUR_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME   // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %r8    // pass Thread::Current()
    call VAR(cxx_name, 1)               // cxx_name(arg1, arg2, arg3, arg4, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME // restore frame up to return address
    CALL_MACRO(return_macro, 2)         // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(ONE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    movq 8(%rsp), %rsi                  // pass referrer
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
                                        // arg0 is in rdi
    movq %gs:THREAD_SELF_OFFSET, %rdx   // pass Thread::Current()
    call VAR(cxx_name, 1)               // cxx_name(arg0, referrer, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME // restore frame up to return address
    CALL_MACRO(return_macro, 2)
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(TWO_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    movq 8(%rsp), %rdx                  // pass referrer
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
                                        // arg0 and arg1 are in rdi/rsi
    movq %gs:THREAD_SELF_OFFSET, %rcx   // pass Thread::Current()
    call VAR(cxx_name, 1)               // (arg0, arg1, referrer, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME // restore frame up to return address
    CALL_MACRO(return_macro, 2)
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(THREE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    movq 8(%rsp), %rcx                  // pass referrer
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
                                        // arg0, arg1, and arg2 are in rdi/rsi/rdx
    movq %gs:THREAD_SELF_OFFSET, %r8    // pass Thread::Current()
    call VAR(cxx_name, 1)               // cxx_name(arg0, arg1, arg2, referrer, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME // restore frame up to return address
    CALL_MACRO(return_macro, 2)         // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO0(RETURN_IF_RESULT_IS_NON_ZERO)
    testq %rax, %rax               // rax == 0 ?
    jz  1f                         // if rax == 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

MACRO0(RETURN_IF_EAX_ZERO)
    testl %eax, %eax               // eax == 0 ?
    jnz  1f                        // if eax != 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

MACRO0(RETURN_OR_DELIVER_PENDING_EXCEPTION)
    movq %gs:THREAD_EXCEPTION_OFFSET, %rcx // get exception field
    testq %rcx, %rcx               // rcx == 0 ?
    jnz 1f                         // if rcx != 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

// Generate the allocation entrypoints for each allocator.
// TODO: use arch/quick_alloc_entrypoints.S. Currently we don't as we need to use concatenation
// macros to work around differences between OS/X's as and binutils as (OS/X lacks named arguments
// to macros and the VAR macro won't concatenate arguments properly), this also breaks having
// multi-line macros that use each other (hence using 1 macro per newline below).
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(c_suffix, cxx_suffix) \
  TWO_ARG_DOWNCALL art_quick_alloc_object ## c_suffix, artAllocObjectFromCode ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(c_suffix, cxx_suffix) \
  TWO_ARG_DOWNCALL art_quick_alloc_object_resolved ## c_suffix, artAllocObjectFromCodeResolved ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(c_suffix, cxx_suffix) \
  TWO_ARG_DOWNCALL art_quick_alloc_object_initialized ## c_suffix, artAllocObjectFromCodeInitialized ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(c_suffix, cxx_suffix) \
  TWO_ARG_DOWNCALL art_quick_alloc_object_with_access_check ## c_suffix, artAllocObjectFromCodeWithAccessCheck ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(c_suffix, cxx_suffix) \
  THREE_ARG_DOWNCALL art_quick_alloc_array ## c_suffix, artAllocArrayFromCode ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(c_suffix, cxx_suffix) \
  THREE_ARG_DOWNCALL art_quick_alloc_array_resolved ## c_suffix, artAllocArrayFromCodeResolved ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(c_suffix, cxx_suffix) \
  THREE_ARG_DOWNCALL art_quick_alloc_array_with_access_check ## c_suffix, artAllocArrayFromCodeWithAccessCheck ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(c_suffix, cxx_suffix) \
  THREE_ARG_DOWNCALL art_quick_check_and_alloc_array ## c_suffix, artCheckAndAllocArrayFromCode ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(c_suffix, cxx_suffix) \
  THREE_ARG_DOWNCALL art_quick_check_and_alloc_array_with_access_check ## c_suffix, artCheckAndAllocArrayFromCodeWithAccessCheck ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(c_suffix, cxx_suffix) \
  FOUR_ARG_DOWNCALL art_quick_alloc_string_from_bytes ## c_suffix, artAllocStringFromBytesFromCode ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(c_suffix, cxx_suffix) \
  THREE_ARG_DOWNCALL art_quick_alloc_string_from_chars ## c_suffix, artAllocStringFromCharsFromCode ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO
#define GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(c_suffix, cxx_suffix) \
  ONE_ARG_DOWNCALL art_quick_alloc_string_from_string ## c_suffix, artAllocStringFromStringFromCode ## cxx_suffix, RETURN_IF_RESULT_IS_NON_ZERO

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_dlmalloc, DlMalloc)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_dlmalloc_instrumented, DlMallocInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_rosalloc, RosAlloc)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_rosalloc_instrumented, RosAllocInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_bump_pointer, BumpPointer)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_bump_pointer_instrumented, BumpPointerInstrumented)

DEFINE_FUNCTION art_quick_alloc_object_tlab
    // Fast path tlab allocation.
    // RDI: uint32_t type_idx, RSI: ArtMethod*
    // RDX, RCX, R8, R9: free. RAX: return val.
    movl ART_METHOD_DEX_CACHE_TYPES_OFFSET(%rsi), %edx  // Load dex cache resolved types array
                                                               // Load the class
    movl MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdx, %rdi, MIRROR_OBJECT_ARRAY_COMPONENT_SIZE), %edx
    testl %edx, %edx                                           // Check null class
    jz   .Lart_quick_alloc_object_tlab_slow_path
                                                               // Check class status.
    cmpl LITERAL(MIRROR_CLASS_STATUS_INITIALIZED), MIRROR_CLASS_STATUS_OFFSET(%rdx)
    jne  .Lart_quick_alloc_object_tlab_slow_path
                                                               // Check access flags has kAccClassIsFinalizable
    testl LITERAL(ACCESS_FLAGS_CLASS_IS_FINALIZABLE), MIRROR_CLASS_ACCESS_FLAGS_OFFSET(%rdx)
    jnz  .Lart_quick_alloc_object_tlab_slow_path
    movl MIRROR_CLASS_OBJECT_SIZE_OFFSET(%rdx), %ecx           // Load the object size.
    addl LITERAL(OBJECT_ALIGNMENT_MASK), %ecx                  // Align the size by 8. (addr + 7) & ~7.
    andl LITERAL(OBJECT_ALIGNMENT_MASK_TOGGLED), %ecx
    movq %gs:THREAD_SELF_OFFSET, %r8                           // r8 = thread
    movq THREAD_LOCAL_POS_OFFSET(%r8), %rax                    // Load thread_local_pos.
    addq %rax, %rcx                                            // Add the object size.
    cmpq THREAD_LOCAL_END_OFFSET(%r8), %rcx                    // Check if it fits.
    ja   .Lart_quick_alloc_object_tlab_slow_path
    movq %rcx, THREAD_LOCAL_POS_OFFSET(%r8)                    // Update thread_local_pos.
    addq LITERAL(1), THREAD_LOCAL_OBJECTS_OFFSET(%r8)          // Increment thread_local_objects.
                                                               // Store the class pointer in the header.
                                                               // No fence needed for x86.
    movl %edx, MIRROR_OBJECT_CLASS_OFFSET(%rax)
    ret                                                        // Fast path succeeded.
.Lart_quick_alloc_object_tlab_slow_path:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdx    // pass Thread::Current()
    call SYMBOL(artAllocObjectFromCodeTLAB)      // cxx_name(arg0, arg1, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_RESULT_IS_NON_ZERO         // return or deliver exception
END_FUNCTION art_quick_alloc_object_tlab

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_tlab, TLAB)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_tlab_instrumented, TLABInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_region, Region)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_region, Region)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_region_instrumented, RegionInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_region_instrumented, RegionInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_region_tlab, RegionTLAB)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_region_tlab_instrumented, RegionTLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_region_tlab_instrumented, RegionTLABInstrumented)

ONE_ARG_DOWNCALL art_quick_resolve_string, artResolveStringFromCode, RETURN_IF_RESULT_IS_NON_ZERO
ONE_ARG_DOWNCALL art_quick_initialize_static_storage, artInitializeStaticStorageFromCode, RETURN_IF_RESULT_IS_NON_ZERO
ONE_ARG_DOWNCALL art_quick_initialize_type, artInitializeTypeFromCode, RETURN_IF_RESULT_IS_NON_ZERO
ONE_ARG_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode, RETURN_IF_RESULT_IS_NON_ZERO

TWO_ARG_REF_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_EAX_ZERO

DEFINE_FUNCTION art_quick_lock_object
    testl %edi, %edi                      // Null check object/rdi.
    jz   .Lslow_lock
.Lretry_lock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi), %ecx  // ecx := lock word.
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx         // Test the 2 high bits.
    jne  .Lslow_lock                      // Slow path if either of the two high bits are set.
    movl %ecx, %edx                       // save lock word (edx) to keep read barrier bits.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), %ecx  // zero the read barrier bits.
    test %ecx, %ecx
    jnz  .Lalready_thin                   // Lock word contains a thin lock.
    // unlocked case - edx: original lock word, edi: obj.
    movl %edx, %eax                       // eax: lock word zero except for read barrier bits.
    movl %gs:THREAD_ID_OFFSET, %edx       // edx := thread id
    or   %eax, %edx                       // edx: thread id with count of 0 + read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)
    jnz  .Lretry_lock                     // cmpxchg failed retry
    ret
.Lalready_thin:  // edx: lock word (with high 2 bits zero and original rb bits), edi: obj.
    movl %gs:THREAD_ID_OFFSET, %ecx       // ecx := thread id
    cmpw %cx, %dx                         // do we hold the lock already?
    jne  .Lslow_lock
    movl %edx, %ecx                       // copy the lock word to check count overflow.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), %ecx  // zero the read barrier bits.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx  // increment recursion count
    test LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK), %ecx  // overflowed if either of the upper two bits (28-29) are set
    jne  .Lslow_lock                      // count overflowed so go slow
    movl %edx, %eax                       // copy the lock word as the old val for cmpxchg.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx   // increment recursion count again for real.
    // update lockword, cmpxchg necessary for read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)  // eax: old val, edx: new val.
    jnz  .Lretry_lock                     // cmpxchg failed retry
    ret
.Lslow_lock:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rsi     // pass Thread::Current()
    call SYMBOL(artLockObjectFromCode)    // artLockObjectFromCode(object, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME   // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object

DEFINE_FUNCTION art_quick_unlock_object
    testl %edi, %edi                      // null check object/edi
    jz   .Lslow_unlock
.Lretry_unlock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi), %ecx  // ecx := lock word
    movl %gs:THREAD_ID_OFFSET, %edx       // edx := thread id
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx
    jnz  .Lslow_unlock                    // lock word contains a monitor
    cmpw %cx, %dx                         // does the thread id match?
    jne  .Lslow_unlock
    movl %ecx, %edx                       // copy the lock word to detect new count of 0.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), %edx  // zero the read barrier bits.
    cmpl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx
    jae  .Lrecursive_thin_unlock
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %ecx, %eax                       // eax: old lock word.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK), %ecx  // ecx: new lock word zero except original rb bits.
#ifndef USE_READ_BARRIER
    movl %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)  // eax: old val, ecx: new val.
    jnz  .Lretry_unlock                   // cmpxchg failed retry
#endif
    ret
.Lrecursive_thin_unlock:  // ecx: original lock word, edi: obj
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %ecx, %eax                       // eax: old lock word.
    subl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx
#ifndef USE_READ_BARRIER
    mov  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)  // eax: old val, ecx: new val.
    jnz  .Lretry_unlock                   // cmpxchg failed retry
#endif
    ret
.Lslow_unlock:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rsi     // pass Thread::Current()
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME   // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object

DEFINE_FUNCTION art_quick_check_cast
    PUSH rdi                          // Save args for exc
    PUSH rsi
    SETUP_FP_CALLEE_SAVE_FRAME
    call SYMBOL(artIsAssignableFromCode)  // (Class* klass, Class* ref_klass)
    testq %rax, %rax
    jz 1f                             // jump forward if not assignable
    RESTORE_FP_CALLEE_SAVE_FRAME
    addq LITERAL(16), %rsp            // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)

    ret
1:
    RESTORE_FP_CALLEE_SAVE_FRAME
    POP rsi                           // Pop arguments
    POP rdi
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %gs:THREAD_SELF_OFFSET, %rdx  // pass Thread::Current()
    call SYMBOL(artThrowClassCastException) // (Class* a, Class* b, Thread*)
    int3                              // unreached
END_FUNCTION art_quick_check_cast


    /*
     * Entry from managed code for array put operations of objects where the value being stored
     * needs to be checked for compatibility.
     *
     * Currently all the parameters should fit into the 32b portions of the registers. Index always
     * will. So we optimize for a tighter encoding. The 64b versions are in comments.
     *
     * rdi(edi) = array, rsi(esi) = index, rdx(edx) = value
     */
DEFINE_FUNCTION art_quick_aput_obj_with_null_and_bound_check
#if defined(__APPLE__)
    int3
    int3
#else
    testl %edi, %edi
//  testq %rdi, %rdi
    jnz art_quick_aput_obj_with_bound_check
    jmp art_quick_throw_null_pointer_exception
#endif  // __APPLE__
END_FUNCTION art_quick_aput_obj_with_null_and_bound_check


DEFINE_FUNCTION art_quick_aput_obj_with_bound_check
#if defined(__APPLE__)
    int3
    int3
#else
    movl MIRROR_ARRAY_LENGTH_OFFSET(%edi), %ecx
//  movl MIRROR_ARRAY_LENGTH_OFFSET(%rdi), %ecx  // This zero-extends, so value(%rcx)=value(%ecx)
    cmpl %ecx, %esi
    jb art_quick_aput_obj
    mov %esi, %edi
//  mov %rsi, %rdi
    mov %ecx, %esi
//  mov %rcx, %rsi
    jmp art_quick_throw_array_bounds
#endif  // __APPLE__
END_FUNCTION art_quick_aput_obj_with_bound_check


DEFINE_FUNCTION art_quick_aput_obj
    testl %edx, %edx                // store of null
//  test %rdx, %rdx
    jz .Ldo_aput_null
    movl MIRROR_OBJECT_CLASS_OFFSET(%edi), %ecx
//  movq MIRROR_OBJECT_CLASS_OFFSET(%rdi), %rcx
    movl MIRROR_CLASS_COMPONENT_TYPE_OFFSET(%ecx), %ecx
//  movq MIRROR_CLASS_COMPONENT_TYPE_OFFSET(%rcx), %rcx
    cmpl MIRROR_OBJECT_CLASS_OFFSET(%edx), %ecx // value's type == array's component type - trivial assignability
//  cmpq MIRROR_CLASS_OFFSET(%rdx), %rcx
    jne .Lcheck_assignability
.Ldo_aput:
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%edi, %esi, 4)
//  movq %rdx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdi, %rsi, 4)
    movq %gs:THREAD_CARD_TABLE_OFFSET, %rdx
    shrl LITERAL(7), %edi
//  shrl LITERAL(7), %rdi
    movb %dl, (%rdx, %rdi)                       // Note: this assumes that top 32b of %rdi are zero
    ret
.Ldo_aput_null:
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%edi, %esi, 4)
//  movq %rdx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdi, %rsi, 4)
    ret
.Lcheck_assignability:
    // Save arguments.
    PUSH rdi
    PUSH rsi
    PUSH rdx
    subq LITERAL(8), %rsp        // Alignment padding.
    CFI_ADJUST_CFA_OFFSET(8)
    SETUP_FP_CALLEE_SAVE_FRAME

                                  // "Uncompress" = do nothing, as already zero-extended on load.
    movl MIRROR_OBJECT_CLASS_OFFSET(%edx), %esi // Pass arg2 = value's class.
    movq %rcx, %rdi               // Pass arg1 = array's component type.

    call SYMBOL(artIsAssignableFromCode)  // (Class* a, Class* b)

    // Exception?
    testq %rax, %rax
    jz   .Lthrow_array_store_exception

    RESTORE_FP_CALLEE_SAVE_FRAME
    // Restore arguments.
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    POP  rdx
    POP  rsi
    POP  rdi

    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%edi, %esi, 4)
//  movq %rdx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdi, %rsi, 4)
    movq %gs:THREAD_CARD_TABLE_OFFSET, %rdx
    shrl LITERAL(7), %edi
//  shrl LITERAL(7), %rdi
    movb %dl, (%rdx, %rdi)                       // Note: this assumes that top 32b of %rdi are zero
//  movb %dl, (%rdx, %rdi)
    ret
    CFI_ADJUST_CFA_OFFSET(32 + 4 * 8)  // Reset unwind info so following code unwinds.
.Lthrow_array_store_exception:
    RESTORE_FP_CALLEE_SAVE_FRAME
    // Restore arguments.
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    POP  rdx
    POP  rsi
    POP  rdi

    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // Save all registers as basis for long jump context.

    // Outgoing argument set up.
    movq %rdx, %rsi                         // Pass arg 2 = value.
    movq %gs:THREAD_SELF_OFFSET, %rdx       // Pass arg 3 = Thread::Current().
                                            // Pass arg 1 = array.
    call SYMBOL(artThrowArrayStoreException) // (array, value, Thread*)
    int3                          // unreached
END_FUNCTION art_quick_aput_obj

// TODO: This is quite silly on X86_64 now.
DEFINE_FUNCTION art_quick_memcpy
    call PLT_SYMBOL(memcpy)       // (void*, const void*, size_t)
    ret
END_FUNCTION art_quick_memcpy

NO_ARG_DOWNCALL art_quick_test_suspend, artTestSuspendFromCode, ret

UNIMPLEMENTED art_quick_ldiv
UNIMPLEMENTED art_quick_lmod
UNIMPLEMENTED art_quick_lmul
UNIMPLEMENTED art_quick_lshl
UNIMPLEMENTED art_quick_lshr
UNIMPLEMENTED art_quick_lushr

THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set64_instance, artSet64InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCode, RETURN_IF_EAX_ZERO

TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get64_instance, artGet64InstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCode, RETURN_IF_EAX_ZERO

ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get64_static, artGet64StaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

// This is singled out as the argument order is different.
DEFINE_FUNCTION art_quick_set64_static
    movq %rsi, %rdx                      // pass new_val
    movq 8(%rsp), %rsi                   // pass referrer
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
                                         // field_idx is in rdi
    movq %gs:THREAD_SELF_OFFSET, %rcx    // pass Thread::Current()
    call SYMBOL(artSet64StaticFromCode)  // (field_idx, referrer, new_val, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO                   // return or deliver exception
END_FUNCTION art_quick_set64_static


DEFINE_FUNCTION art_quick_proxy_invoke_handler
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_RDI

    movq %gs:THREAD_SELF_OFFSET, %rdx       // Pass Thread::Current().
    movq %rsp, %rcx                         // Pass SP.
    call SYMBOL(artQuickProxyInvokeHandler) // (proxy method, receiver, Thread*, SP)
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    movq %rax, %xmm0                        // Copy return value in case of float returns.
    RETURN_OR_DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict.
     * rax is a hidden argument that holds the target method's dex method index.
     */
DEFINE_FUNCTION art_quick_imt_conflict_trampoline
#if defined(__APPLE__)
    int3
    int3
#else
    movq 8(%rsp), %rdi            // load caller Method*
    movl ART_METHOD_DEX_CACHE_METHODS_OFFSET(%rdi), %edi     // load dex_cache_resolved_methods
    movq MIRROR_LONG_ARRAY_DATA_OFFSET(%rdi, %rax, 8), %rdi  // load the target method
    jmp art_quick_invoke_interface_trampoline
#endif  // __APPLE__
END_FUNCTION art_quick_imt_conflict_trampoline

DEFINE_FUNCTION art_quick_resolution_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rdx
    movq %rsp, %rcx
    call SYMBOL(artQuickResolutionTrampoline) // (called, receiver, Thread*, SP)
    movq %rax, %r10               // Remember returned code pointer in R10.
    movq (%rsp), %rdi             // Load called method into RDI.
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    testq %r10, %r10              // If code pointer is null goto deliver pending exception.
    jz 1f
    jmp *%r10                     // Tail call into method.
1:
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_resolution_trampoline

/* Generic JNI frame layout:
 *
 * #-------------------#
 * |                   |
 * | caller method...  |
 * #-------------------#    <--- SP on entry
 *
 *          |
 *          V
 *
 * #-------------------#
 * | caller method...  |
 * #-------------------#
 * | Return            |
 * | R15               |    callee save
 * | R14               |    callee save
 * | R13               |    callee save
 * | R12               |    callee save
 * | R9                |    arg5
 * | R8                |    arg4
 * | RSI/R6            |    arg1
 * | RBP/R5            |    callee save
 * | RBX/R3            |    callee save
 * | RDX/R2            |    arg2
 * | RCX/R1            |    arg3
 * | XMM7              |    float arg 8
 * | XMM6              |    float arg 7
 * | XMM5              |    float arg 6
 * | XMM4              |    float arg 5
 * | XMM3              |    float arg 4
 * | XMM2              |    float arg 3
 * | XMM1              |    float arg 2
 * | XMM0              |    float arg 1
 * | RDI/Method*       |  <- sp
 * #-------------------#
 * | Scratch Alloca    |    5K scratch space
 * #---------#---------#
 * |         | sp*     |
 * | Tramp.  #---------#
 * | args    | thread  |
 * | Tramp.  #---------#
 * |         | method  |
 * #-------------------#    <--- SP on artQuickGenericJniTrampoline
 *
 *           |
 *           v              artQuickGenericJniTrampoline
 *
 * #-------------------#
 * | caller method...  |
 * #-------------------#
 * | Return            |
 * | Callee-Save Data  |
 * #-------------------#
 * | handle scope      |
 * #-------------------#
 * | Method*           |    <--- (1)
 * #-------------------#
 * | local ref cookie  | // 4B
 * | handle scope size | // 4B   TODO: roll into call stack alignment?
 * #-------------------#
 * | JNI Call Stack    |
 * #-------------------#    <--- SP on native call
 * |                   |
 * | Stack for Regs    |    The trampoline assembly will pop these values
 * |                   |    into registers for native call
 * #-------------------#
 * | Native code ptr   |
 * #-------------------#
 * | Free scratch      |
 * #-------------------#
 * | Ptr to (1)        |    <--- RSP
 * #-------------------#
 */
    /*
     * Called to do a generic JNI down-call
     */
DEFINE_FUNCTION art_quick_generic_jni_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_RDI

    movq %rsp, %rbp                 // save SP at (old) callee-save frame
    CFI_DEF_CFA_REGISTER(rbp)

    //
    // reserve a lot of space
    //
    //      4    local state ref
    //      4    padding
    //   4196    4k scratch space, enough for 2x 256 8-byte parameters (TODO: handle scope overhead?)
    //     16    handle scope member fields ?
    // +  112    14x 8-byte stack-2-register space
    // ------
    //   4332
    // 16-byte aligned: 4336
    // Note: 14x8 = 7*16, so the stack stays aligned for the native call...
    //       Also means: the padding is somewhere in the middle
    //
    //
    // New test: use 5K and release
    // 5k = 5120
    subq LITERAL(5120), %rsp
    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //    rdi    rsi      <= C calling convention
    //  gs:...   rbp      <= where they are
    movq %gs:THREAD_SELF_OFFSET, %rdi
    movq %rbp, %rsi
    call SYMBOL(artQuickGenericJniTrampoline)  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // %rax: pointer to native code, 0 on error.
    // %rdx: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    test %rax, %rax
    jz .Lexception_in_native

    // Release part of the alloca.
    movq %rdx, %rsp

    // pop from the register-passing alloca region
    // what's the right layout?
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    // TODO: skip floating point if unused, some flag.
    movq 0(%rsp), %xmm0
    movq 8(%rsp), %xmm1
    movq 16(%rsp), %xmm2
    movq 24(%rsp), %xmm3
    movq 32(%rsp), %xmm4
    movq 40(%rsp), %xmm5
    movq 48(%rsp), %xmm6
    movq 56(%rsp), %xmm7
    addq LITERAL(64), %rsp          // floating-point done

    // native call
    call *%rax

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*,  result, result_f)
    //   rdi      rsi   rdx       <= C calling convention
    //  gs:...    rax   xmm0      <= where they are
    movq %gs:THREAD_SELF_OFFSET, %rdi
    movq %rax, %rsi
    movq %xmm0, %rdx
    call SYMBOL(artQuickGenericJniEndTrampoline)

    // Pending exceptions possible.
    // TODO: use cmpq, needs direct encoding because of gas bug
    movq %gs:THREAD_EXCEPTION_OFFSET, %rcx
    test %rcx, %rcx
    jnz .Lexception_in_native

    // Tear down the alloca.
    movq %rbp, %rsp
    CFI_DEF_CFA_REGISTER(rsp)

    // Tear down the callee-save frame.
    // Load FPRs.
    // movq %xmm0, 16(%rsp)         // doesn't make sense!!!
    movq 24(%rsp), %xmm1            // neither does this!!!
    movq 32(%rsp), %xmm2
    movq 40(%rsp), %xmm3
    movq 48(%rsp), %xmm4
    movq 56(%rsp), %xmm5
    movq 64(%rsp), %xmm6
    movq 72(%rsp), %xmm7
    movq 80(%rsp), %xmm12
    movq 88(%rsp), %xmm13
    movq 96(%rsp), %xmm14
    movq 104(%rsp), %xmm15
    // was 80 bytes
    addq LITERAL(80 + 4*8), %rsp
    CFI_ADJUST_CFA_OFFSET(-80 - 4*8)
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    POP rcx  // Arg.
    POP rdx  // Arg.
    POP rbx  // Callee save.
    POP rbp  // Callee save.
    POP rsi  // Arg.
    POP r8   // Arg.
    POP r9   // Arg.
    POP r12  // Callee save.
    POP r13  // Callee save.
    POP r14  // Callee save.
    POP r15  // Callee save.
    // store into fpr, for when it's a fpr return...
    movq %rax, %xmm0
    ret
.Lexception_in_native:
    movq %gs:THREAD_TOP_QUICK_FRAME_OFFSET, %rsp
    CFI_DEF_CFA_REGISTER(rsp)
    // Do a call to push a new save-all frame required by the runtime.
    call .Lexception_call
.Lexception_call:
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_generic_jni_trampoline

    /*
     * Called to bridge from the quick to interpreter ABI. On entry the arguments match those
     * of a quick call:
     * RDI = method being called / to bridge to.
     * RSI, RDX, RCX, R8, R9 are arguments to that method.
     */
DEFINE_FUNCTION art_quick_to_interpreter_bridge
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME   // Set up frame and save arguments.
    movq %gs:THREAD_SELF_OFFSET, %rsi      // RSI := Thread::Current()
    movq %rsp, %rdx                        // RDX := sp
    call SYMBOL(artQuickToInterpreterBridge)  // (method, Thread*, SP)
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME  // TODO: no need to restore arguments in this case.
    movq %rax, %xmm0                   // Place return value also into floating point return value.
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
DEFINE_FUNCTION art_quick_instrumentation_entry
#if defined(__APPLE__)
    int3
    int3
#else
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME

    movq %rdi, %r12               // Preserve method pointer in a callee-save.

    movq %gs:THREAD_SELF_OFFSET, %rdx   // Pass thread.
    movq FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE-8(%rsp), %rcx   // Pass return PC.

    call SYMBOL(artInstrumentationMethodEntryFromCode) // (Method*, Object*, Thread*, LR)

                                  // %rax = result of call.
    movq %r12, %rdi               // Reload method pointer.

    leaq art_quick_instrumentation_exit(%rip), %r12   // Set up return through instrumentation
    movq %r12, FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE-8(%rsp) // exit.

    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME

    jmp *%rax                     // Tail call to intended method.
#endif  // __APPLE__
END_FUNCTION art_quick_instrumentation_entry

DEFINE_FUNCTION art_quick_instrumentation_exit
    pushq LITERAL(0)          // Push a fake return PC as there will be none on the stack.

    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME

    // We need to save rax and xmm0. We could use a callee-save from SETUP_REF_ONLY, but then
    // we would need to fully restore it. As there are a good number of callee-save registers, it
    // seems easier to have an extra small stack area. But this should be revisited.

    movq  %rsp, %rsi                          // Pass SP.

    PUSH rax                  // Save integer result.
    subq LITERAL(8), %rsp     // Save floating-point result.
    CFI_ADJUST_CFA_OFFSET(8)
    movq %xmm0, (%rsp)

    movq  %gs:THREAD_SELF_OFFSET, %rdi        // Pass Thread.
    movq  %rax, %rdx                          // Pass integer result.
    movq  %xmm0, %rcx                         // Pass floating-point result.

    call SYMBOL(artInstrumentationMethodExitFromCode)   // (Thread*, SP, gpr_res, fpr_res)

    movq  %rax, %rdi          // Store return PC
    movq  %rdx, %rsi          // Store second return PC in hidden arg.

    movq (%rsp), %xmm0        // Restore floating-point result.
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    POP rax                   // Restore integer result.

    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME

    addq LITERAL(8), %rsp     // Drop fake return pc.

    jmp   *%rdi               // Return.
END_FUNCTION art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
DEFINE_FUNCTION art_quick_deoptimize
    pushq %rsi                     // Entry point for a jump. Fake that we were called.
                                   // Use hidden arg.
.globl SYMBOL(art_quick_deoptimize_from_compiled_slow_path)  // Entry point for real calls
                                                             // from compiled slow paths.
SYMBOL(art_quick_deoptimize_from_compiled_slow_path):
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
                                   // Stack should be aligned now.
    movq %gs:THREAD_SELF_OFFSET, %rdi         // Pass Thread.
    call SYMBOL(artDeoptimize) // artDeoptimize(Thread*)
    int3                           // Unreachable.
END_FUNCTION art_quick_deoptimize

    /*
     * String's compareTo.
     *
     * On entry:
     *    rdi:   this string object (known non-null)
     *    rsi:   comp string object (known non-null)
     */
DEFINE_FUNCTION art_quick_string_compareto
    movl MIRROR_STRING_COUNT_OFFSET(%edi), %r8d
    movl MIRROR_STRING_COUNT_OFFSET(%esi), %r9d
    /* Build pointers to the start of string data */
    leal MIRROR_STRING_VALUE_OFFSET(%edi), %edi
    leal MIRROR_STRING_VALUE_OFFSET(%esi), %esi
    /* Calculate min length and count diff */
    movl  %r8d, %ecx
    movl  %r8d, %eax
    subl  %r9d, %eax
    cmovg %r9d, %ecx
    /*
     * At this point we have:
     *   eax: value to return if first part of strings are equal
     *   ecx: minimum among the lengths of the two strings
     *   esi: pointer to comp string data
     *   edi: pointer to this string data
     */
    jecxz .Lkeep_length
    repe cmpsw                    // find nonmatching chars in [%esi] and [%edi], up to length %ecx
    jne .Lnot_equal
.Lkeep_length:
    ret
    .balign 16
.Lnot_equal:
    movzwl  -2(%edi), %eax        // get last compared char from this string
    movzwl  -2(%esi), %ecx        // get last compared char from comp string
    subl  %ecx, %eax              // return the difference
    ret
END_FUNCTION art_quick_string_compareto

UNIMPLEMENTED art_quick_memcmp16

DEFINE_FUNCTION art_quick_assignable_from_code
    SETUP_FP_CALLEE_SAVE_FRAME
    call SYMBOL(artIsAssignableFromCode)       // (const mirror::Class*, const mirror::Class*)
    RESTORE_FP_CALLEE_SAVE_FRAME
    ret
END_FUNCTION art_quick_assignable_from_code


// Return from a nested signal:
// Entry:
//  rdi: address of jmp_buf in TLS

DEFINE_FUNCTION art_nested_signal_return
                                    // first arg to longjmp is already in correct register
    movq LITERAL(1), %rsi           // second arg to longjmp (1)
    call PLT_SYMBOL(longjmp)
    int3                            // won't get here
END_FUNCTION art_nested_signal_return
