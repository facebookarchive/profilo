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

#include "asm_support_x86.S"

// For x86, the CFA is esp+4, the address above the pushed return address on the stack.

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     */
MACRO2(SETUP_SAVE_ALL_CALLEE_SAVE_FRAME, got_reg, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl  MACRO_LITERAL(12), %esp  // Grow stack by 3 words.
    CFI_ADJUST_CFA_OFFSET(12)
    SETUP_GOT_NOSAVE RAW_VAR(got_reg, 0)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg, 0)), REG_VAR(temp_reg, 1)
    movl (REG_VAR(temp_reg, 1)), REG_VAR(temp_reg, 1)
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET(REG_VAR(temp_reg, 1))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVE != 3*4 + 16 + 4)
#error "SAVE_ALL_CALLEE_SAVE_FRAME(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly)
     */
MACRO2(SETUP_REFS_ONLY_CALLEE_SAVE_FRAME, got_reg, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl  MACRO_LITERAL(12), %esp  // Grow stack by 3 words.
    CFI_ADJUST_CFA_OFFSET(12)
    SETUP_GOT_NOSAVE VAR(got_reg, 0)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg, 0)), REG_VAR(temp_reg, 1)
    movl (REG_VAR(temp_reg, 1)), REG_VAR(temp_reg, 1)
    // Push save all callee-save method.
    pushl RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET(REG_VAR(temp_reg, 1))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE != 3*4 + 16 + 4)
#error "REFS_ONLY_CALLEE_SAVE_FRAME(X86) size not as expected."
#endif
END_MACRO

MACRO0(RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME)
    addl MACRO_LITERAL(16), %esp  // Unwind stack up to saved values
    CFI_ADJUST_CFA_OFFSET(-16)
    POP ebp  // Restore callee saves (ebx is saved/restored by the upcall)
    POP esi
    POP edi
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs)
     */
MACRO2(SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME, got_reg, temp_reg)
    PUSH edi  // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx
    // Create space for FPR args.
    subl MACRO_LITERAL(4 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    // Save FPRs.
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)

    SETUP_GOT_NOSAVE VAR(got_reg, 0)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg, 0)), REG_VAR(temp_reg, 1)
    movl (REG_VAR(temp_reg, 1)), REG_VAR(temp_reg, 1)
    // Push save all callee-save method.
    pushl RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET(REG_VAR(temp_reg, 1))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the stop quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE != 7*4 + 4*8 + 4)
#error "REFS_AND_ARGS_CALLEE_SAVE_FRAME(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs) where the method is passed in EAX.
     */
MACRO0(SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_EAX)
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH edi  // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx

    // Create space for FPR args.
    subl MACRO_LITERAL(32), %esp
    CFI_ADJUST_CFA_OFFSET(32)

    // Save FPRs.
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)

    PUSH eax  // Store the ArtMethod reference at the bottom of the stack.
    // Store esp as the stop quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
END_MACRO

MACRO0(RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME)
    // Restore FPRs. EAX is still on the stack.
    movsd 4(%esp), %xmm0
    movsd 12(%esp), %xmm1
    movsd 20(%esp), %xmm2
    movsd 28(%esp), %xmm3

    addl MACRO_LITERAL(36), %esp  // Remove FPRs and EAX.
    CFI_ADJUST_CFA_OFFSET(-36)

    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    POP edi
END_MACRO

// Restore register and jump to routine
// Inputs:  EDI contains pointer to code.
// Notes: Need to pop EAX too (restores Method*)
MACRO0(RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME_AND_JUMP)
    POP eax  // Restore Method*

    // Restore FPRs.
    movsd 0(%esp), %xmm0
    movsd 8(%esp), %xmm1
    movsd 16(%esp), %xmm2
    movsd 24(%esp), %xmm3

    addl MACRO_LITERAL(32), %esp  // Remove FPRs.
    CFI_ADJUST_CFA_OFFSET(-32)

    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    xchgl 0(%esp),%edi // restore EDI and place code pointer as only value on stack
    ret
END_MACRO

    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME ebx, ebx  // save callee saves for throw
    // Outgoing argument set up
    subl  MACRO_LITERAL(12), %esp              // Alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*)
    int3                                       // unreached
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  ebx, ebx  // save all registers as basis for long jump context
    // Outgoing argument set up
    subl  MACRO_LITERAL(12), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call VAR(cxx_name, 1)         // cxx_name(Thread*)
    int3                          // unreached
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO2(ONE_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME ebx, ebx  // save all registers as basis for long jump context
    mov %esp, %ecx
    // Outgoing argument set up
    subl  MACRO_LITERAL(8), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, Thread*)
    int3                          // unreached
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO2(TWO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME ebx, ebx  // save all registers as basis for long jump context
    // Outgoing argument set up
    PUSH eax                      // alignment padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, Thread*)
    int3                          // unreached
    END_FUNCTION RAW_VAR(c_name, 0)
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
     * as usual - except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, load the caller's Method*, align the
     * stack and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/r1.
     *
     * The helper will attempt to locate the target and return a 64-bit result in r0/r1 consisting
     * of the target Method* in r0 and method->code_ in r1.
     *
     * If unsuccessful, the helper will return null/null will bea pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
MACRO2(INVOKE_TRAMPOLINE, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME ebx, ebx
    movl %esp, %edx  // remember SP

    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    pushl 32+32(%edx)             // pass caller Method*
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, arg3, Thread*, SP)
    movl %edx, %edi               // save code pointer in EDI
    addl MACRO_LITERAL(36), %esp  // Pop arguments skip eax
    CFI_ADJUST_CFA_OFFSET(-36)

    // Restore FPRs.
    movsd 0(%esp), %xmm0
    movsd 8(%esp), %xmm1
    movsd 16(%esp), %xmm2
    movsd 24(%esp), %xmm3

    // Remove space for FPR args.
    addl MACRO_LITERAL(4 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(-4 * 8)

    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    // Swap EDI callee save with code pointer.
    xchgl %edi, (%esp)
    testl %eax, %eax              // Branch forward if exception pending.
    jz    1f
    // Tail call to intended method.
    ret
1:
    addl MACRO_LITERAL(4), %esp   // Pop code pointer off stack
    CFI_ADJUST_CFA_OFFSET(-4)
    DELIVER_PENDING_EXCEPTION
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline, artInvokeInterfaceTrampoline
INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck

    /*
     * Helper for quick invocation stub to set up XMM registers.
     * Increments shorty and arg_array and clobbers temp_char.
     * Branches to finished if it encounters the end of the shorty.
     */
MACRO5(LOOP_OVER_SHORTY_LOADING_XMMS, xmm_reg, shorty, arg_array, temp_char, finished)
1: // LOOP
    movb (REG_VAR(shorty, 1)), REG_VAR(temp_char, 3)  // temp_char := *shorty
    addl MACRO_LITERAL(1), REG_VAR(shorty, 1)         // shorty++
    cmpb MACRO_LITERAL(0), REG_VAR(temp_char, 3)      // if (temp_char == '\0')
    je RAW_VAR(finished, 4)                           //   goto finished
    cmpb MACRO_LITERAL(68), REG_VAR(temp_char, 3)     // if (temp_char == 'D')
    je 2f                                             //   goto FOUND_DOUBLE
    cmpb MACRO_LITERAL(70), REG_VAR(temp_char, 3)     // if (temp_char == 'F')
    je 3f                                             //   goto FOUND_FLOAT
    addl MACRO_LITERAL(4), REG_VAR(arg_array, 2)      // arg_array++
    //  Handle extra space in arg array taken by a long.
    cmpb MACRO_LITERAL(74), REG_VAR(temp_char, 3)     // if (temp_char != 'J')
    jne 1b                                            //   goto LOOP
    addl MACRO_LITERAL(4), REG_VAR(arg_array, 2)      // arg_array++
    jmp 1b                                            // goto LOOP
2:  // FOUND_DOUBLE
    movsd (REG_VAR(arg_array, 2)), REG_VAR(xmm_reg, 0)
    addl MACRO_LITERAL(8), REG_VAR(arg_array, 2)      // arg_array+=2
    jmp 4f
3:  // FOUND_FLOAT
    movss (REG_VAR(arg_array, 2)), REG_VAR(xmm_reg, 0)
    addl MACRO_LITERAL(4), REG_VAR(arg_array, 2)      // arg_array++
4:
END_MACRO

    /*
     * Helper for quick invocation stub to set up GPR registers.
     * Increments shorty and arg_array, and returns the current short character in
     * temp_char. Branches to finished if it encounters the end of the shorty.
     */
MACRO4(SKIP_OVER_FLOATS, shorty, arg_array, temp_char, finished)
1: // LOOP:
    movb (REG_VAR(shorty, 0)), REG_VAR(temp_char, 2)  // temp_char := *shorty
    addl MACRO_LITERAL(1), REG_VAR(shorty, 0)         // shorty++
    cmpb MACRO_LITERAL(0), REG_VAR(temp_char, 2)      // if (temp_char == '\0')
    je RAW_VAR(finished, 3)                           //   goto finished
    cmpb MACRO_LITERAL(70), REG_VAR(temp_char, 2)     // if (temp_char == 'F')
    je 3f                                             //   goto SKIP_FLOAT
    cmpb MACRO_LITERAL(68), REG_VAR(temp_char, 2)     // if (temp_char == 'D')
    je 4f                                             //   goto SKIP_DOUBLE
    jmp 5f                                            // goto end
3:  // SKIP_FLOAT
    addl MACRO_LITERAL(4), REG_VAR(arg_array, 1)      // arg_array++
    jmp 1b                                            // goto LOOP
4:  // SKIP_DOUBLE
    addl MACRO_LITERAL(8), REG_VAR(arg_array, 1)      // arg_array+=2
    jmp 1b                                            // goto LOOP
5:
END_MACRO

  /*
     * Quick invocation stub (non-static).
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = method pointer
     *   [sp + 8] = argument array or null for no argument methods
     *   [sp + 12] = size of argument array in bytes
     *   [sp + 16] = (managed) thread pointer
     *   [sp + 20] = JValue* result
     *   [sp + 24] = shorty
     */
DEFINE_FUNCTION art_quick_invoke_stub
    // Save the non-volatiles.
    PUSH ebp                      // save ebp
    PUSH ebx                      // save ebx
    PUSH esi                      // save esi
    PUSH edi                      // save edi
    // Set up argument XMM registers.
    mov 24+16(%esp), %esi         // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 8+16(%esp), %edi          // EDI := arg_array + 4 ; ie skip this pointer.
    addl LITERAL(4), %edi
    // Clobbers ESI, EDI, EAX.
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, esi, edi, al, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, esi, edi, al, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, esi, edi, al, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, esi, edi, al, .Lxmm_setup_finished
    .balign 16
.Lxmm_setup_finished:
    mov %esp, %ebp                // copy value of stack pointer into base pointer
    CFI_DEF_CFA_REGISTER(ebp)
    mov 28(%ebp), %ebx            // get arg array size
    // reserve space for return addr, method*, ebx, ebp, esi, and edi in frame
    addl LITERAL(36), %ebx
    // align frame size to 16 bytes
    andl LITERAL(0xFFFFFFF0), %ebx
    subl LITERAL(20), %ebx        // remove space for return address, ebx, ebp, esi and edi
    subl %ebx, %esp               // reserve stack space for argument array

    movl LITERAL(0), (%esp)       // store null for method*

    // Copy arg array into stack.
    movl 28(%ebp), %ecx           // ECX = size of args
    movl 24(%ebp), %esi           // ESI = argument array
    leal 4(%esp), %edi            // EDI = just after Method* in stack arguments
    rep movsb                     // while (ecx--) { *edi++ = *esi++ }

    mov 40(%ebp), %esi            // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 24(%ebp), %edi            // EDI := arg_array
    mov 0(%edi), %ecx             // ECX := this pointer
    addl LITERAL(4), %edi         // EDI := arg_array + 4 ; ie skip this pointer.

    // Enumerate the possible cases for loading GPRS.
    // edx (and maybe ebx):
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished
    cmpb LITERAL(74), %al         // if (al == 'J') goto FOUND_LONG
    je .LfirstLong
    // Must be an integer value.
    movl (%edi), %edx
    addl LITERAL(4), %edi         // arg_array++

    // Now check ebx
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished
    // Must be first word of a long, or an integer. First word of long doesn't
    // go into EBX, but can be loaded there anyways, as it is harmless.
    movl (%edi), %ebx
    jmp .Lgpr_setup_finished
.LfirstLong:
    movl (%edi), %edx
    movl 4(%edi), %ebx
    // Nothing left to load.
.Lgpr_setup_finished:
    mov 20(%ebp), %eax            // move method pointer into eax
    call *ART_METHOD_QUICK_CODE_OFFSET_32(%eax) // call the method
    mov %ebp, %esp                // restore stack pointer
    CFI_DEF_CFA_REGISTER(esp)
    POP edi                       // pop edi
    POP esi                       // pop esi
    POP ebx                       // pop ebx
    POP ebp                       // pop ebp
    mov 20(%esp), %ecx            // get result pointer
    mov %eax, (%ecx)              // store the result assuming its a long, int or Object*
    mov %edx, 4(%ecx)             // store the other half of the result
    mov 24(%esp), %edx            // get the shorty
    cmpb LITERAL(68), (%edx)      // test if result type char == 'D'
    je .Lreturn_double_quick
    cmpb LITERAL(70), (%edx)      // test if result type char == 'F'
    je .Lreturn_float_quick
    ret
.Lreturn_double_quick:
    movsd %xmm0, (%ecx)           // store the floating point result
    ret
.Lreturn_float_quick:
    movss %xmm0, (%ecx)           // store the floating point result
    ret
END_FUNCTION art_quick_invoke_stub

  /*
     * Quick invocation stub (static).
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = method pointer
     *   [sp + 8] = argument array or null for no argument methods
     *   [sp + 12] = size of argument array in bytes
     *   [sp + 16] = (managed) thread pointer
     *   [sp + 20] = JValue* result
     *   [sp + 24] = shorty
     */
DEFINE_FUNCTION art_quick_invoke_static_stub
    // Save the non-volatiles.
    PUSH ebp                      // save ebp
    PUSH ebx                      // save ebx
    PUSH esi                      // save esi
    PUSH edi                      // save edi
    // Set up argument XMM registers.
    mov 24+16(%esp), %esi         // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 8+16(%esp), %edi          // EDI := arg_array
    // Clobbers ESI, EDI, EAX.
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, esi, edi, al, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, esi, edi, al, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, esi, edi, al, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, esi, edi, al, .Lxmm_setup_finished2
    .balign 16
.Lxmm_setup_finished2:
    mov %esp, %ebp                // copy value of stack pointer into base pointer
    CFI_DEF_CFA_REGISTER(ebp)
    mov 28(%ebp), %ebx            // get arg array size
    // reserve space for return addr, method*, ebx, ebp, esi, and edi in frame
    addl LITERAL(36), %ebx
    // align frame size to 16 bytes
    andl LITERAL(0xFFFFFFF0), %ebx
    subl LITERAL(20), %ebx        // remove space for return address, ebx, ebp, esi and edi
    subl %ebx, %esp               // reserve stack space for argument array

    movl LITERAL(0), (%esp)       // store null for method*

    // Copy arg array into stack.
    movl 28(%ebp), %ecx           // ECX = size of args
    movl 24(%ebp), %esi           // ESI = argument array
    leal 4(%esp), %edi            // EDI = just after Method* in stack arguments
    rep movsb                     // while (ecx--) { *edi++ = *esi++ }

    mov 40(%ebp), %esi            // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 24(%ebp), %edi            // EDI := arg_array

    // Enumerate the possible cases for loading GPRS.
    // ecx (and maybe edx)
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    cmpb LITERAL(74), %al         // if (al == 'J') goto FOUND_LONG
    je .LfirstLong2
    // Must be an integer value.  Load into ECX.
    movl (%edi), %ecx
    addl LITERAL(4), %edi         // arg_array++

    // Now check edx (and maybe ebx).
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    cmpb LITERAL(74), %al         // if (al == 'J') goto FOUND_LONG
    je .LSecondLong2
    // Must be an integer.  Load into EDX.
    movl (%edi), %edx
    addl LITERAL(4), %edi         // arg_array++

    // Is there anything for ebx?
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    // Must be first word of a long, or an integer. First word of long doesn't
    // go into EBX, but can be loaded there anyways, as it is harmless.
    movl (%edi), %ebx
    jmp .Lgpr_setup_finished2
.LSecondLong2:
    // EDX:EBX is long.  That is all.
    movl (%edi), %edx
    movl 4(%edi), %ebx
    jmp .Lgpr_setup_finished2
.LfirstLong2:
    // ECX:EDX is a long
    movl (%edi), %ecx
    movl 4(%edi), %edx
    addl LITERAL(8), %edi         // arg_array += 2

    // Anything for EBX?
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    // Must be first word of a long, or an integer. First word of long doesn't
    // go into EBX, but can be loaded there anyways, as it is harmless.
    movl (%edi), %ebx
    jmp .Lgpr_setup_finished2
    // Nothing left to load.
.Lgpr_setup_finished2:
    mov 20(%ebp), %eax            // move method pointer into eax
    call *ART_METHOD_QUICK_CODE_OFFSET_32(%eax) // call the method
    mov %ebp, %esp                // restore stack pointer
    CFI_DEF_CFA_REGISTER(esp)
    POP edi                       // pop edi
    POP esi                       // pop esi
    POP ebx                       // pop ebx
    POP ebp                       // pop ebp
    mov 20(%esp), %ecx            // get result pointer
    mov %eax, (%ecx)              // store the result assuming its a long, int or Object*
    mov %edx, 4(%ecx)             // store the other half of the result
    mov 24(%esp), %edx            // get the shorty
    cmpb LITERAL(68), (%edx)      // test if result type char == 'D'
    je .Lreturn_double_quick2
    cmpb LITERAL(70), (%edx)      // test if result type char == 'F'
    je .Lreturn_float_quick2
    ret
.Lreturn_double_quick2:
    movsd %xmm0, (%ecx)           // store the floating point result
    ret
.Lreturn_float_quick2:
    movss %xmm0, (%ecx)           // store the floating point result
    ret
END_FUNCTION art_quick_invoke_static_stub

MACRO3(NO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp  // push padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call VAR(cxx_name, 1)         // cxx_name(Thread*)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(ONE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp   // push padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, Thread*)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(TWO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    PUSH eax                      // push padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, Thread*)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(THREE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, arg3, Thread*)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(FOUR_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                      // pass arg4
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, arg3, arg4, Thread*)
    addl MACRO_LITERAL(32), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(ONE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx // save ref containing registers for GC
    // Outgoing argument set up
    mov FRAME_SIZE_REFS_ONLY_CALLEE_SAVE(%esp), %ecx  // get referrer
    PUSH eax                      // push padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, referrer, Thread*)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(TWO_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME ebx, ebx // save ref containing registers for GC
    // Outgoing argument set up
    mov FRAME_SIZE_REFS_ONLY_CALLEE_SAVE(%esp), %edx  // get referrer
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, referrer, Thread*)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(THREE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    mov FRAME_SIZE_REFS_ONLY_CALLEE_SAVE(%esp), %ebx  // get referrer
    subl MACRO_LITERAL(12), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                      // pass referrer
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, arg3, referrer, Thread*)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO0(RETURN_IF_RESULT_IS_NON_ZERO)
    testl %eax, %eax               // eax == 0 ?
    jz  1f                         // if eax == 0 goto 1
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
    cmpl MACRO_LITERAL(0),%fs:THREAD_EXCEPTION_OFFSET // exception field == 0 ?
    jne 1f                         // if exception field != 0 goto 1
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

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_tlab, TLAB)
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
    testl %eax, %eax                      // null check object/eax
    jz   .Lslow_lock
.Lretry_lock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%eax), %ecx  // ecx := lock word
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx         // test the 2 high bits.
    jne  .Lslow_lock                      // slow path if either of the two high bits are set.
    movl %ecx, %edx                       // save lock word (edx) to keep read barrier bits.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), %ecx  // zero the read barrier bits.
    test %ecx, %ecx
    jnz  .Lalready_thin                   // lock word contains a thin lock
    // unlocked case - edx: original lock word, eax: obj.
    movl %eax, %ecx                       // remember object in case of retry
    movl %edx, %eax                       // eax: lock word zero except for read barrier bits.
    movl %fs:THREAD_ID_OFFSET, %edx       // load thread id.
    or   %eax, %edx                       // edx: thread id with count of 0 + read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%ecx)  // eax: old val, edx: new val.
    jnz  .Llock_cmpxchg_fail              // cmpxchg failed retry
    ret
.Lalready_thin:  // edx: lock word (with high 2 bits zero and original rb bits), eax: obj.
    movl %fs:THREAD_ID_OFFSET, %ecx       // ecx := thread id
    cmpw %cx, %dx                         // do we hold the lock already?
    jne  .Lslow_lock
    movl %edx, %ecx                       // copy the lock word to check count overflow.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), %ecx  // zero the read barrier bits.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx  // increment recursion count for overflow check.
    test LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK), %ecx  // overflowed if either of the upper two bits (28-29) are set.
    jne  .Lslow_lock                      // count overflowed so go slow
    movl %eax, %ecx                       // save obj to use eax for cmpxchg.
    movl %edx, %eax                       // copy the lock word as the old val for cmpxchg.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx  // increment recursion count again for real.
    // update lockword, cmpxchg necessary for read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%ecx)  // eax: old val, edx: new val.
    jnz  .Llock_cmpxchg_fail              // cmpxchg failed retry
    ret
.Llock_cmpxchg_fail:
    movl  %ecx, %eax                      // restore eax
    jmp  .Lretry_lock
.Lslow_lock:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass object
    call SYMBOL(artLockObjectFromCode)  // artLockObjectFromCode(object, Thread*)
    addl LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object

DEFINE_FUNCTION art_quick_unlock_object
    testl %eax, %eax                      // null check object/eax
    jz   .Lslow_unlock
.Lretry_unlock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%eax), %ecx  // ecx := lock word
    movl %fs:THREAD_ID_OFFSET, %edx       // edx := thread id
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx
    jnz  .Lslow_unlock                    // lock word contains a monitor
    cmpw %cx, %dx                         // does the thread id match?
    jne  .Lslow_unlock
    movl %ecx, %edx                       // copy the lock word to detect new count of 0.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED), %edx  // zero the read barrier bits.
    cmpl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx
    jae  .Lrecursive_thin_unlock
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %eax, %edx                       // edx: obj
    movl %ecx, %eax                       // eax: old lock word.
    andl LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK), %ecx  // ecx: new lock word zero except original rb bits.
#ifndef USE_READ_BARRIER
    movl %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)  // eax: old val, ecx: new val.
    jnz  .Lunlock_cmpxchg_fail            // cmpxchg failed retry
#endif
    ret
.Lrecursive_thin_unlock:  // ecx: original lock word, eax: obj
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %eax, %edx                       // edx: obj
    movl %ecx, %eax                       // eax: old lock word.
    subl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx  // ecx: new lock word with decremented count.
#ifndef USE_READ_BARRIER
    mov  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)  // eax: old val, ecx: new val.
    jnz  .Lunlock_cmpxchg_fail            // cmpxchg failed retry
#endif
    ret
.Lunlock_cmpxchg_fail:  // edx: obj
    movl %edx, %eax                       // restore eax
    jmp  .Lretry_unlock
.Lslow_unlock:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass object
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*)
    addl LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object

DEFINE_FUNCTION art_quick_is_assignable
    PUSH eax                     // alignment padding
    PUSH ecx                     // pass arg2 - obj->klass
    PUSH eax                     // pass arg1 - checked class
    call SYMBOL(artIsAssignableFromCode)  // (Class* klass, Class* ref_klass)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_is_assignable

DEFINE_FUNCTION art_quick_check_cast
    PUSH eax                     // alignment padding
    PUSH ecx                     // pass arg2 - obj->klass
    PUSH eax                     // pass arg1 - checked class
    call SYMBOL(artIsAssignableFromCode)  // (Class* klass, Class* ref_klass)
    testl %eax, %eax
    jz 1f                         // jump forward if not assignable
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret

    CFI_ADJUST_CFA_OFFSET(12)     // Reset unwind info so following code unwinds.
1:
    POP eax                       // pop arguments
    POP ecx
    addl LITERAL(4), %esp
    CFI_ADJUST_CFA_OFFSET(-4)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  ebx, ebx  // save all registers as basis for long jump context
    // Outgoing argument set up
    PUSH eax                      // alignment padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call SYMBOL(artThrowClassCastException) // (Class* a, Class* b, Thread*)
    int3                          // unreached
END_FUNCTION art_quick_check_cast

    /*
     * Entry from managed code for array put operations of objects where the value being stored
     * needs to be checked for compatibility.
     * eax = array, ecx = index, edx = value
     */
DEFINE_FUNCTION art_quick_aput_obj_with_null_and_bound_check
    testl %eax, %eax
    jnz SYMBOL(art_quick_aput_obj_with_bound_check)
    jmp SYMBOL(art_quick_throw_null_pointer_exception)
END_FUNCTION art_quick_aput_obj_with_null_and_bound_check

DEFINE_FUNCTION art_quick_aput_obj_with_bound_check
    movl MIRROR_ARRAY_LENGTH_OFFSET(%eax), %ebx
    cmpl %ebx, %ecx
    jb SYMBOL(art_quick_aput_obj)
    mov %ecx, %eax
    mov %ebx, %ecx
    jmp SYMBOL(art_quick_throw_array_bounds)
END_FUNCTION art_quick_aput_obj_with_bound_check

DEFINE_FUNCTION art_quick_aput_obj
    test %edx, %edx              // store of null
    jz .Ldo_aput_null
    movl MIRROR_OBJECT_CLASS_OFFSET(%eax), %ebx
    movl MIRROR_CLASS_COMPONENT_TYPE_OFFSET(%ebx), %ebx
    // value's type == array's component type - trivial assignability
    cmpl MIRROR_OBJECT_CLASS_OFFSET(%edx), %ebx
    jne .Lcheck_assignability
.Ldo_aput:
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)
    movl %fs:THREAD_CARD_TABLE_OFFSET, %edx
    shrl LITERAL(7), %eax
    movb %dl, (%edx, %eax)
    ret
.Ldo_aput_null:
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)
    ret
.Lcheck_assignability:
    PUSH eax                     // save arguments
    PUSH ecx
    PUSH edx
    subl LITERAL(8), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl MIRROR_OBJECT_CLASS_OFFSET(%edx)  // pass arg2 - type of the value to be stored
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                      // pass arg1 - component type of the array
    call SYMBOL(artIsAssignableFromCode)  // (Class* a, Class* b)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    testl %eax, %eax
    jz   .Lthrow_array_store_exception
    POP  edx
    POP  ecx
    POP  eax
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)  // do the aput
    movl %fs:THREAD_CARD_TABLE_OFFSET, %edx
    shrl LITERAL(7), %eax
    movb %dl, (%edx, %eax)
    ret
    CFI_ADJUST_CFA_OFFSET(12)     // 3 POP after the jz for unwinding.
.Lthrow_array_store_exception:
    POP  edx
    POP  ecx
    POP  eax
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME ebx, ebx // save all registers as basis for long jump context
    // Outgoing argument set up
    PUSH eax                      // alignment padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass arg2 - value
    PUSH eax                      // pass arg1 - array
    call SYMBOL(artThrowArrayStoreException) // (array, value, Thread*)
    int3                          // unreached
END_FUNCTION art_quick_aput_obj

DEFINE_FUNCTION art_quick_memcpy
    SETUP_GOT_NOSAVE ebx          // clobbers EBX
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call PLT_SYMBOL(memcpy)       // (void*, const void*, size_t)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_memcpy

NO_ARG_DOWNCALL art_quick_test_suspend, artTestSuspendFromCode, ret

DEFINE_FUNCTION art_quick_d2l
    subl LITERAL(12), %esp        // alignment padding, room for argument
    CFI_ADJUST_CFA_OFFSET(12)
    movsd %xmm0, 0(%esp)          // arg a
    call SYMBOL(art_d2l)          // (jdouble a)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_d2l

DEFINE_FUNCTION art_quick_f2l
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    movss %xmm0, 0(%esp)          // arg a
    call SYMBOL(art_f2l)          // (jfloat a)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_f2l

DEFINE_FUNCTION art_quick_ldiv
    subl LITERAL(12), %esp       // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                     // pass arg4 b.hi
    PUSH edx                     // pass arg3 b.lo
    PUSH ecx                     // pass arg2 a.hi
    PUSH eax                     // pass arg1 a.lo
    call SYMBOL(artLdiv)     // (jlong a, jlong b)
    addl LITERAL(28), %esp       // pop arguments
    CFI_ADJUST_CFA_OFFSET(-28)
    ret
END_FUNCTION art_quick_ldiv

DEFINE_FUNCTION art_quick_lmod
    subl LITERAL(12), %esp       // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                     // pass arg4 b.hi
    PUSH edx                     // pass arg3 b.lo
    PUSH ecx                     // pass arg2 a.hi
    PUSH eax                     // pass arg1 a.lo
    call SYMBOL(artLmod)     // (jlong a, jlong b)
    addl LITERAL(28), %esp       // pop arguments
    CFI_ADJUST_CFA_OFFSET(-28)
    ret
END_FUNCTION art_quick_lmod

DEFINE_FUNCTION art_quick_lmul
    imul %eax, %ebx              // ebx = a.lo(eax) * b.hi(ebx)
    imul %edx, %ecx              // ecx = b.lo(edx) * a.hi(ecx)
    mul  %edx                    // edx:eax = a.lo(eax) * b.lo(edx)
    add  %ebx, %ecx
    add  %ecx, %edx              // edx += (a.lo * b.hi) + (b.lo * a.hi)
    ret
END_FUNCTION art_quick_lmul

DEFINE_FUNCTION art_quick_lshl
    // ecx:eax << edx
    xchg %edx, %ecx
    shld %cl,%eax,%edx
    shl  %cl,%eax
    test LITERAL(32), %cl
    jz  1f
    mov %eax, %edx
    xor %eax, %eax
1:
    ret
END_FUNCTION art_quick_lshl

DEFINE_FUNCTION art_quick_lshr
    // ecx:eax >> edx
    xchg %edx, %ecx
    shrd %cl,%edx,%eax
    sar  %cl,%edx
    test LITERAL(32),%cl
    jz  1f
    mov %edx, %eax
    sar LITERAL(31), %edx
1:
    ret
END_FUNCTION art_quick_lshr

DEFINE_FUNCTION art_quick_lushr
    // ecx:eax >>> edx
    xchg %edx, %ecx
    shrd %cl,%edx,%eax
    shr  %cl,%edx
    test LITERAL(32),%cl
    jz  1f
    mov %edx, %eax
    xor %edx, %edx
1:
    ret
END_FUNCTION art_quick_lushr

ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get64_static, artGet64StaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get64_instance, artGet64InstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCode, RETURN_IF_EAX_ZERO

THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCode, RETURN_IF_EAX_ZERO

// Call artSet64InstanceFromCode with 4 word size arguments and the referrer.
DEFINE_FUNCTION art_quick_set64_instance
    movd %ebx, %xmm0
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME ebx, ebx  // save ref containing registers for GC
    movd %xmm0, %ebx
    // Outgoing argument set up
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    pushl (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE+12)(%esp)  // pass referrer
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                      // pass high half of new_val
    PUSH edx                      // pass low half of new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet64InstanceFromCode)  // (field_idx, Object*, new_val, referrer, Thread*)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_instance

// Call artSet64StaticFromCode with 3 word size arguments plus with the referrer in the 2nd position
// so that new_val is aligned on even registers were we passing arguments in registers.
DEFINE_FUNCTION art_quick_set64_static
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  ebx, ebx  // save ref containing registers for GC
    mov FRAME_SIZE_REFS_ONLY_CALLEE_SAVE(%esp), %ebx  // get referrer
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass high half of new_val
    PUSH ecx                      // pass low half of new_val
    PUSH ebx                      // pass referrer
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet64StaticFromCode)  // (field_idx, referrer, new_val, Thread*)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_static

DEFINE_FUNCTION art_quick_proxy_invoke_handler
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_EAX
    PUSH esp                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass proxy method
    call SYMBOL(artQuickProxyInvokeHandler) // (proxy method, receiver, Thread*, SP)
    movd %eax, %xmm0              // place return value also into floating point return value
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    addl LITERAL(16 + FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE - FRAME_SIZE_REFS_ONLY_CALLEE_SAVE), %esp
    CFI_ADJUST_CFA_OFFSET(-(16 + FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE - FRAME_SIZE_REFS_ONLY_CALLEE_SAVE))
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict. xmm0 is a hidden argument that holds the target method's
     * dex method index.
     */
DEFINE_FUNCTION art_quick_imt_conflict_trampoline
    PUSH ecx
    movl 8(%esp), %eax            // load caller Method*
    movl ART_METHOD_DEX_CACHE_METHODS_OFFSET(%eax), %eax  // load dex_cache_resolved_methods
    movd %xmm7, %ecx              // get target method index stored in xmm0
    movl MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4), %eax  // load the target method
    POP ecx
    jmp SYMBOL(art_quick_invoke_interface_trampoline)
END_FUNCTION art_quick_imt_conflict_trampoline

DEFINE_FUNCTION art_quick_resolution_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME ebx, ebx
    movl %esp, %edi
    PUSH EDI                      // pass SP. do not just PUSH ESP; that messes up unwinding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass method
    call SYMBOL(artQuickResolutionTrampoline) // (Method* called, receiver, Thread*, SP)
    movl %eax, %edi               // remember code pointer in EDI
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    test %eax, %eax               // if code pointer is null goto deliver pending exception
    jz 1f
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME_AND_JUMP
1:
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_resolution_trampoline

DEFINE_FUNCTION art_quick_generic_jni_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_EAX
    movl %esp, %ebp                 // save SP at callee-save frame
    CFI_DEF_CFA_REGISTER(ebp)
    subl LITERAL(5120), %esp
    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //  (esp)    4(esp)   <= C calling convention
    //  fs:...    ebp     <= where they are

    subl LITERAL(8), %esp         // Padding for 16B alignment.
    pushl %ebp                    // Pass SP (to ArtMethod).
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    call SYMBOL(artQuickGenericJniTrampoline)  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // eax: pointer to native code, 0 on error.
    // edx: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    test %eax, %eax
    jz .Lexception_in_native

    // Release part of the alloca.
    movl %edx, %esp

    // On x86 there are no registers passed, so nothing to pop here.
    // Native call.
    call *%eax

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*, result, result_f)
    //  (esp)    4(esp)  12(esp)    <= C calling convention
    //  fs:...  eax:edx   fp0      <= where they are

    subl LITERAL(20), %esp         // Padding & pass float result.
    fstpl (%esp)
    pushl %edx                    // Pass int result.
    pushl %eax
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    call SYMBOL(artQuickGenericJniEndTrampoline)

    // Pending exceptions possible.
    mov %fs:THREAD_EXCEPTION_OFFSET, %ebx
    testl %ebx, %ebx
    jnz .Lexception_in_native

    // Tear down the alloca.
    movl %ebp, %esp
    CFI_DEF_CFA_REGISTER(esp)


    // Tear down the callee-save frame.
    // Remove space for FPR args and EAX
    addl LITERAL(4 + 4 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(-(4 + 4 * 8))

    POP ecx
    addl LITERAL(4), %esp     // Avoid edx, as it may be part of the result.
    CFI_ADJUST_CFA_OFFSET(-4)
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    POP edi
    // Quick expects the return value to be in xmm0.
    movd %eax, %xmm0
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    ret
.Lexception_in_native:
    movl %fs:THREAD_TOP_QUICK_FRAME_OFFSET, %esp
    // Do a call to push a new save-all frame required by the runtime.
    call .Lexception_call
.Lexception_call:
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_generic_jni_trampoline

DEFINE_FUNCTION art_quick_to_interpreter_bridge
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME  ebx, ebx  // save frame
    mov %esp, %edx                // remember SP
    PUSH eax                      // alignment padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass  method
    call SYMBOL(artQuickToInterpreterBridge)  // (method, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)

    // Return eax:edx in xmm0 also.
    movd %eax, %xmm0
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0

    addl LITERAL(48), %esp        // Remove FPRs and EAX, ECX, EDX, EBX.
    CFI_ADJUST_CFA_OFFSET(-48)

    POP ebp  // Restore callee saves
    POP esi
    POP edi

    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
DEFINE_FUNCTION art_quick_instrumentation_entry
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME ebx, edx
    PUSH eax                      // Save eax which will be clobbered by the callee-save method.
    subl LITERAL(12), %esp        // Align stack.
    CFI_ADJUST_CFA_OFFSET(12)
    pushl FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE-4+16(%esp)  // Pass LR.
    CFI_ADJUST_CFA_OFFSET(4)
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // Pass receiver.
    PUSH eax                      // Pass Method*.
    call SYMBOL(artInstrumentationMethodEntryFromCode) // (Method*, Object*, Thread*, LR)
    addl LITERAL(28), %esp        // Pop arguments upto saved Method*.
    CFI_ADJUST_CFA_OFFSET(-28)
    movl 60(%esp), %edi           // Restore edi.
    movl %eax, 60(%esp)           // Place code* over edi, just under return pc.
    movl SYMBOL(art_quick_instrumentation_exit)@GOT(%ebx), %ebx
    // Place instrumentation exit as return pc. ebx holds the GOT computed on entry.
    movl %ebx, 64(%esp)
    movl 0(%esp), %eax           // Restore eax.
    // Restore FPRs (extra 4 bytes of offset due to EAX push at top).
    movsd 8(%esp), %xmm0
    movsd 16(%esp), %xmm1
    movsd 24(%esp), %xmm2
    movsd 32(%esp), %xmm3

    // Restore GPRs.
    movl 40(%esp), %ecx           // Restore ecx.
    movl 44(%esp), %edx           // Restore edx.
    movl 48(%esp), %ebx           // Restore ebx.
    movl 52(%esp), %ebp           // Restore ebp.
    movl 56(%esp), %esi           // Restore esi.
    addl LITERAL(60), %esp        // Wind stack back upto code*.
    CFI_ADJUST_CFA_OFFSET(-60)
    ret                           // Call method (and pop).
END_FUNCTION art_quick_instrumentation_entry

DEFINE_FUNCTION art_quick_instrumentation_exit
    pushl LITERAL(0)              // Push a fake return PC as there will be none on the stack.
    CFI_ADJUST_CFA_OFFSET(4)
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME ebx, ebx
    mov  %esp, %ecx               // Remember SP
    subl LITERAL(8), %esp         // Save float return value.
    CFI_ADJUST_CFA_OFFSET(8)
    movq %xmm0, (%esp)
    PUSH edx                      // Save gpr return value.
    PUSH eax
    subl LITERAL(16), %esp        // Align stack
    CFI_ADJUST_CFA_OFFSET(16)
    movq %xmm0, (%esp)            // Pass float return value.
    PUSH edx                      // Pass gpr return value.
    PUSH eax
    PUSH ecx                      // Pass SP.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current.
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artInstrumentationMethodExitFromCode)  // (Thread*, SP, gpr_result, fpr_result)
    mov   %eax, %ecx              // Move returned link register.
    addl LITERAL(32), %esp        // Pop arguments.
    CFI_ADJUST_CFA_OFFSET(-32)
    movl %edx, %ebx               // Move returned link register for deopt
                                  // (ebx is pretending to be our LR).
    POP eax                       // Restore gpr return value.
    POP edx
    movq (%esp), %xmm0            // Restore fpr return value.
    addl LITERAL(8), %esp
    CFI_ADJUST_CFA_OFFSET(-8)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    addl LITERAL(4), %esp         // Remove fake return pc.
    CFI_ADJUST_CFA_OFFSET(-4)
    jmp   *%ecx                   // Return.
END_FUNCTION art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
DEFINE_FUNCTION art_quick_deoptimize
    PUSH ebx                      // Entry point for a jump. Fake that we were called.
.globl SYMBOL(art_quick_deoptimize_from_compiled_slow_path)  // Entry point for real calls
                                                             // from compiled slow paths.
SYMBOL(art_quick_deoptimize_from_compiled_slow_path):
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME ebx, ebx
    subl LITERAL(12), %esp        // Align stack.
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artDeoptimize)    // artDeoptimize(Thread*)
    int3                          // Unreachable.
END_FUNCTION art_quick_deoptimize

    /*
     * String's compareTo.
     *
     * On entry:
     *    eax:   this string object (known non-null)
     *    ecx:   comp string object (known non-null)
     */
DEFINE_FUNCTION art_quick_string_compareto
    PUSH esi                    // push callee save reg
    PUSH edi                    // push callee save reg
    mov MIRROR_STRING_COUNT_OFFSET(%eax), %edx
    mov MIRROR_STRING_COUNT_OFFSET(%ecx), %ebx
    lea MIRROR_STRING_VALUE_OFFSET(%eax), %esi
    lea MIRROR_STRING_VALUE_OFFSET(%ecx), %edi
    /* Calculate min length and count diff */
    mov   %edx, %ecx
    mov   %edx, %eax
    subl  %ebx, %eax
    cmovg %ebx, %ecx
    /*
     * At this point we have:
     *   eax: value to return if first part of strings are equal
     *   ecx: minimum among the lengths of the two strings
     *   esi: pointer to this string data
     *   edi: pointer to comp string data
     */
    jecxz .Lkeep_length
    repe cmpsw                    // find nonmatching chars in [%esi] and [%edi], up to length %ecx
    jne .Lnot_equal
.Lkeep_length:
    POP edi                       // pop callee save reg
    POP esi                       // pop callee save reg
    ret
    .balign 16
.Lnot_equal:
    movzwl  -2(%esi), %eax        // get last compared char from this string
    movzwl  -2(%edi), %ecx        // get last compared char from comp string
    subl  %ecx, %eax              // return the difference
    POP edi                       // pop callee save reg
    POP esi                       // pop callee save reg
    ret
END_FUNCTION art_quick_string_compareto

// Return from a nested signal:
// Entry:
//  eax: address of jmp_buf in TLS

DEFINE_FUNCTION art_nested_signal_return
    SETUP_GOT_NOSAVE ebx            // sets %ebx for call into PLT
    movl LITERAL(1), %ecx
    PUSH ecx                        // second arg to longjmp (1)
    PUSH eax                        // first arg to longjmp (jmp_buf)
    call PLT_SYMBOL(longjmp)
    int3                            // won't get here.
END_FUNCTION art_nested_signal_return

    // TODO: implement these!
UNIMPLEMENTED art_quick_memcmp16
