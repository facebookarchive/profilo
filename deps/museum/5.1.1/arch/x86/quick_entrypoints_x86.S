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
MACRO0(SETUP_SAVE_ALL_CALLEE_SAVE_FRAME)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl  MACRO_LITERAL(16), %esp  // Grow stack by 4 words, bottom word will hold Method*
    CFI_ADJUST_CFA_OFFSET(16)
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
MACRO0(SETUP_REF_ONLY_CALLEE_SAVE_FRAME)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl  MACRO_LITERAL(16), %esp  // Grow stack by 4 words, bottom word will hold Method*
    CFI_ADJUST_CFA_OFFSET(16)

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE != 3*4 + 16 + 4)
#error "REFS_ONLY_CALLEE_SAVE_FRAME(X86) size not as expected."
#endif
END_MACRO

MACRO0(RESTORE_REF_ONLY_CALLEE_SAVE_FRAME)
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
MACRO0(SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME)
    PUSH edi  // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx
    PUSH eax   // Align stack, eax will be clobbered by Method*

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE != 7*4 + 4)
#error "REFS_AND_ARGS_CALLEE_SAVE_FRAME(X86) size not as expected."
#endif
END_MACRO

MACRO0(RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME)
    addl MACRO_LITERAL(4), %esp  // Remove padding
    CFI_ADJUST_CFA_OFFSET(-4)
    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    POP edi
END_MACRO

    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME         // save callee saves for throw
    mov %esp, %ecx
    // Outgoing argument set up
    subl  MACRO_LITERAL(8), %esp             // Alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH ecx                                 // pass SP
    pushl %fs:THREAD_SELF_OFFSET             // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    SETUP_GOT_NOSAVE                         // clobbers ebx (harmless here)
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*, SP)
    int3                                     // unreached
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %ecx
    // Outgoing argument set up
    subl  MACRO_LITERAL(8), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH ecx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    SETUP_GOT_NOSAVE              // clobbers ebx (harmless here)
    call VAR(cxx_name, 1)     // cxx_name(Thread*, SP)
    int3                          // unreached
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO2(ONE_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %ecx
    // Outgoing argument set up
    PUSH eax                      // alignment padding
    PUSH ecx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass arg1
    SETUP_GOT_NOSAVE              // clobbers ebx (harmless here)
    call VAR(cxx_name, 1)     // cxx_name(arg1, Thread*, SP)
    int3                          // unreached
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO2(TWO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %edx
    // Outgoing argument set up
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    SETUP_GOT_NOSAVE              // clobbers ebx (harmless here)
    call VAR(cxx_name, 1)     // cxx_name(arg1, arg2, Thread*, SP)
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
     * If unsuccessful, the helper will return NULL/NULL. There will bea pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
MACRO2(INVOKE_TRAMPOLINE, c_name, cxx_name)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    // Set up the callee save frame to conform with Runtime::CreateCalleeSaveMethod(kRefsAndArgs)
    // return address
    PUSH edi
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx
    PUSH eax    // <-- callee save Method* to go here
    movl %esp, %edx  // remember SP
    // Outgoing argument set up
    SETUP_GOT_NOSAVE
    subl MACRO_LITERAL(12), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    pushl 32(%edx)                // pass caller Method*
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)     // cxx_name(arg1, arg2, arg3, Thread*, SP)
    movl %edx, %edi               // save code pointer in EDI
    addl MACRO_LITERAL(36), %esp  // Pop arguments skip eax
    CFI_ADJUST_CFA_OFFSET(-36)
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
     * Quick invocation stub.
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = method pointer
     *   [sp + 8] = argument array or NULL for no argument methods
     *   [sp + 12] = size of argument array in bytes
     *   [sp + 16] = (managed) thread pointer
     *   [sp + 20] = JValue* result
     *   [sp + 24] = shorty
     */
DEFINE_FUNCTION art_quick_invoke_stub
    PUSH ebp                      // save ebp
    PUSH ebx                      // save ebx
    mov %esp, %ebp                // copy value of stack pointer into base pointer
    CFI_DEF_CFA_REGISTER(ebp)
    mov 20(%ebp), %ebx            // get arg array size
    addl LITERAL(28), %ebx        // reserve space for return addr, method*, ebx, and ebp in frame
    andl LITERAL(0xFFFFFFF0), %ebx    // align frame size to 16 bytes
    subl LITERAL(12), %ebx        // remove space for return address, ebx, and ebp
    subl %ebx, %esp               // reserve stack space for argument array
    SETUP_GOT_NOSAVE              // clobbers ebx (harmless here)
    lea  4(%esp), %eax            // use stack pointer + method ptr as dest for memcpy
    pushl 20(%ebp)                // push size of region to memcpy
    pushl 16(%ebp)                // push arg array as source of memcpy
    pushl %eax                    // push stack pointer as destination of memcpy
    call PLT_SYMBOL(memcpy)       // (void*, const void*, size_t)
    addl LITERAL(12), %esp        // pop arguments to memcpy
    movl LITERAL(0), (%esp)       // store NULL for method*
    mov 12(%ebp), %eax            // move method pointer into eax
    mov 4(%esp), %ecx             // copy arg1 into ecx
    mov 8(%esp), %edx             // copy arg2 into edx
    mov 12(%esp), %ebx            // copy arg3 into ebx
    call *METHOD_QUICK_CODE_OFFSET_32(%eax) // call the method
    mov %ebp, %esp                // restore stack pointer
    CFI_DEF_CFA_REGISTER(esp)
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

MACRO3(NO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    SETUP_GOT_NOSAVE              // clobbers ebx (harmless here)
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp   // push padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call VAR(cxx_name, 1)     // cxx_name(Thread*, SP)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(ONE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    SETUP_GOT_NOSAVE              // clobbers EBX
    // Outgoing argument set up
    PUSH eax                      // push padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)     // cxx_name(arg1, Thread*, SP)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(TWO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    SETUP_GOT_NOSAVE              // clobbers EBX
    // Outgoing argument set up
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)     // cxx_name(arg1, arg2, Thread*, SP)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION RAW_VAR(c_name, 0)
END_MACRO

MACRO3(THREE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION RAW_VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp  // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    SETUP_GOT_NOSAVE              // clobbers EBX
    call VAR(cxx_name, 1)     // cxx_name(arg1, arg2, arg3, Thread*, SP)
    addl MACRO_LITERAL(32), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
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
    mov %fs:THREAD_EXCEPTION_OFFSET, %ebx // get exception field
    testl %ebx, %ebx               // ebx == 0 ?
    jnz 1f                         // if ebx != 0 goto 1
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

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_dlmalloc, DlMalloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc, DlMalloc)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_dlmalloc_instrumented, DlMallocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_dlmalloc_instrumented, DlMallocInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_rosalloc, RosAlloc)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc, RosAlloc)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_rosalloc_instrumented, RosAllocInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_rosalloc_instrumented, RosAllocInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_bump_pointer, BumpPointer)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer, BumpPointer)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_bump_pointer_instrumented, BumpPointerInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_bump_pointer_instrumented, BumpPointerInstrumented)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab, TLAB)

GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY(_tlab_instrumented, TLABInstrumented)
GENERATE_ALLOC_ENTRYPOINTS_CHECK_AND_ALLOC_ARRAY_WITH_ACCESS_CHECK(_tlab_instrumented, TLABInstrumented)

TWO_ARG_DOWNCALL art_quick_resolve_string, artResolveStringFromCode, RETURN_IF_RESULT_IS_NON_ZERO
TWO_ARG_DOWNCALL art_quick_initialize_static_storage, artInitializeStaticStorageFromCode, RETURN_IF_RESULT_IS_NON_ZERO
TWO_ARG_DOWNCALL art_quick_initialize_type, artInitializeTypeFromCode, RETURN_IF_RESULT_IS_NON_ZERO
TWO_ARG_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode, RETURN_IF_RESULT_IS_NON_ZERO

TWO_ARG_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_EAX_ZERO

DEFINE_FUNCTION art_quick_lock_object
    testl %eax, %eax                      // null check object/eax
    jz   .Lslow_lock
.Lretry_lock:
    movl LOCK_WORD_OFFSET(%eax), %ecx     // ecx := lock word
    test LITERAL(0xC0000000), %ecx        // test the 2 high bits.
    jne  .Lslow_lock                      // slow path if either of the two high bits are set.
    movl %fs:THREAD_ID_OFFSET, %edx       // edx := thread id
    test %ecx, %ecx
    jnz  .Lalready_thin                   // lock word contains a thin lock
    // unlocked case - %edx holds thread id with count of 0
    movl %eax, %ecx                       // remember object in case of retry
    xor  %eax, %eax                       // eax == 0 for comparison with lock word in cmpxchg
    lock cmpxchg  %edx, LOCK_WORD_OFFSET(%ecx)
    jnz  .Lcmpxchg_fail                   // cmpxchg failed retry
    ret
.Lcmpxchg_fail:
    movl  %ecx, %eax                       // restore eax
    jmp  .Lretry_lock
.Lalready_thin:
    cmpw %cx, %dx                         // do we hold the lock already?
    jne  .Lslow_lock
    addl LITERAL(65536), %ecx             // increment recursion count
    test LITERAL(0xC0000000), %ecx        // overflowed if either of top two bits are set
    jne  .Lslow_lock                      // count overflowed so go slow
    movl %ecx, LOCK_WORD_OFFSET(%eax)     // update lockword, cmpxchg not necessary as we hold lock
    ret
.Lslow_lock:
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    SETUP_GOT_NOSAVE              // clobbers EBX
    // Outgoing argument set up
    PUSH eax                      // push padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass object
    call SYMBOL(artLockObjectFromCode)  // artLockObjectFromCode(object, Thread*, SP)
    addl LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object

DEFINE_FUNCTION art_quick_unlock_object
    testl %eax, %eax                      // null check object/eax
    jz   .Lslow_unlock
    movl LOCK_WORD_OFFSET(%eax), %ecx     // ecx := lock word
    movl %fs:THREAD_ID_OFFSET, %edx       // edx := thread id
    test LITERAL(0xC0000000), %ecx
    jnz  .Lslow_unlock                    // lock word contains a monitor
    cmpw %cx, %dx                         // does the thread id match?
    jne  .Lslow_unlock
    cmpl LITERAL(65536), %ecx
    jae  .Lrecursive_thin_unlock
    movl LITERAL(0), LOCK_WORD_OFFSET(%eax)
    ret
.Lrecursive_thin_unlock:
    subl LITERAL(65536), %ecx
    mov  %ecx, LOCK_WORD_OFFSET(%eax)
    ret
.Lslow_unlock:
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    SETUP_GOT_NOSAVE              // clobbers EBX
    // Outgoing argument set up
    PUSH eax                      // push padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass object
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*, SP)
    addl LITERAL(16), %esp  // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object

DEFINE_FUNCTION art_quick_is_assignable
    SETUP_GOT_NOSAVE             // clobbers EBX
    PUSH eax                     // alignment padding
    PUSH ecx                     // pass arg2 - obj->klass
    PUSH eax                     // pass arg1 - checked class
    call SYMBOL(artIsAssignableFromCode)  // (Class* klass, Class* ref_klass)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_is_assignable

DEFINE_FUNCTION art_quick_check_cast
    SETUP_GOT_NOSAVE             // clobbers EBX
    PUSH eax                     // alignment padding
    PUSH ecx                     // pass arg2 - obj->klass
    PUSH eax                     // pass arg1 - checked class
    call SYMBOL(artIsAssignableFromCode)  // (Class* klass, Class* ref_klass)
    testl %eax, %eax
    jz 1f                         // jump forward if not assignable
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
1:
    POP eax                       // pop arguments
    POP ecx
    addl LITERAL(4), %esp
    CFI_ADJUST_CFA_OFFSET(-12)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %edx
    // Outgoing argument set up
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call SYMBOL(artThrowClassCastException) // (Class* a, Class* b, Thread*, SP)
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
    movl ARRAY_LENGTH_OFFSET(%eax), %ebx
    cmpl %ebx, %ecx
    jb SYMBOL(art_quick_aput_obj)
    mov %ecx, %eax
    mov %ebx, %ecx
    jmp SYMBOL(art_quick_throw_array_bounds)
END_FUNCTION art_quick_aput_obj_with_bound_check

DEFINE_FUNCTION art_quick_aput_obj
    test %edx, %edx              // store of null
    jz .Ldo_aput_null
    movl CLASS_OFFSET(%eax), %ebx
    movl CLASS_COMPONENT_TYPE_OFFSET(%ebx), %ebx
    cmpl CLASS_OFFSET(%edx), %ebx // value's type == array's component type - trivial assignability
    jne .Lcheck_assignability
.Ldo_aput:
    movl %edx, OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)
    movl %fs:THREAD_CARD_TABLE_OFFSET, %edx
    shrl LITERAL(7), %eax
    movb %dl, (%edx, %eax)
    ret
.Ldo_aput_null:
    movl %edx, OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)
    ret
.Lcheck_assignability:
    PUSH eax                     // save arguments
    PUSH ecx
    PUSH edx
    subl LITERAL(8), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl CLASS_OFFSET(%edx)     // pass arg2 - type of the value to be stored
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                     // pass arg1 - component type of the array
    SETUP_GOT_NOSAVE             // clobbers EBX
    call SYMBOL(artIsAssignableFromCode)  // (Class* a, Class* b)
    addl LITERAL(16), %esp       // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    testl %eax, %eax
    jz   .Lthrow_array_store_exception
    POP  edx
    POP  ecx
    POP  eax
    movl %edx, OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)  // do the aput
    movl %fs:THREAD_CARD_TABLE_OFFSET, %edx
    shrl LITERAL(7), %eax
    movb %dl, (%edx, %eax)
    ret
.Lthrow_array_store_exception:
    POP  edx
    POP  ecx
    POP  eax
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %ecx
    // Outgoing argument set up
    PUSH ecx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass arg2 - value
    PUSH eax                      // pass arg1 - array
    call SYMBOL(artThrowArrayStoreException) // (array, value, Thread*, SP)
    int3                          // unreached
END_FUNCTION art_quick_aput_obj

DEFINE_FUNCTION art_quick_memcpy
    SETUP_GOT_NOSAVE              // clobbers EBX
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
    PUSH eax                      // alignment padding
    PUSH ecx                      // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(art_d2l)      // (jdouble a)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_d2l

DEFINE_FUNCTION art_quick_f2l
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    SETUP_GOT_NOSAVE              // clobbers EBX
    PUSH eax                      // pass arg1 a
    call SYMBOL(art_f2l)      // (jfloat a)
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
    SETUP_GOT_NOSAVE             // clobbers EBX
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
    SETUP_GOT_NOSAVE             // clobbers EBX
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

DEFINE_FUNCTION art_quick_set32_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    mov 32(%ebx), %ebx            // get referrer
    PUSH ebx                      // pass referrer
    PUSH edx                      // pass new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artSet32InstanceFromCode)  // (field_idx, Object*, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set32_instance

DEFINE_FUNCTION art_quick_set64_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH esp                      // pass SP-8
    addl LITERAL(8), (%esp)       // fix SP on stack by adding 8
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                      // pass high half of new_val
    PUSH edx                      // pass low half of new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artSet64InstanceFromCode)  // (field_idx, Object*, new_val, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_instance

DEFINE_FUNCTION art_quick_set_obj_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    mov 32(%ebx), %ebx            // get referrer
    PUSH ebx                      // pass referrer
    PUSH edx                      // pass new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artSetObjInstanceFromCode) // (field_idx, Object*, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set_obj_instance

DEFINE_FUNCTION art_quick_get32_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artGet32InstanceFromCode)  // (field_idx, Object*, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get32_instance

DEFINE_FUNCTION art_quick_get64_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artGet64InstanceFromCode)  // (field_idx, Object*, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get64_instance

DEFINE_FUNCTION art_quick_get_obj_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artGetObjInstanceFromCode) // (field_idx, Object*, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get_obj_instance

DEFINE_FUNCTION art_quick_set32_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass new_val
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artSet32StaticFromCode)  // (field_idx, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set32_static

DEFINE_FUNCTION art_quick_set64_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    mov 32(%ebx), %ebx            // get referrer
    PUSH edx                      // pass high half of new_val
    PUSH ecx                      // pass low half of new_val
    PUSH ebx                      // pass referrer
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artSet64StaticFromCode)  // (field_idx, referrer, new_val, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_static

DEFINE_FUNCTION art_quick_set_obj_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass new_val
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artSetObjStaticFromCode)  // (field_idx, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set_obj_static

DEFINE_FUNCTION art_quick_get32_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    mov 32(%esp), %ecx            // get referrer
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artGet32StaticFromCode)    // (field_idx, referrer, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get32_static

DEFINE_FUNCTION art_quick_get64_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    mov 32(%esp), %ecx            // get referrer
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artGet64StaticFromCode)    // (field_idx, referrer, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get64_static

DEFINE_FUNCTION art_quick_get_obj_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    mov 32(%esp), %ecx            // get referrer
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass field_idx
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artGetObjStaticFromCode)   // (field_idx, referrer, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get_obj_static

DEFINE_FUNCTION art_quick_proxy_invoke_handler
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME   // save frame and Method*
    PUSH esp                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass proxy method
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artQuickProxyInvokeHandler) // (proxy method, receiver, Thread*, SP)
    movd %eax, %xmm0              // place return value also into floating point return value
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    addl LITERAL(44), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-44)
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict. xmm0 is a hidden argument that holds the target method's
     * dex method index.
     */
DEFINE_FUNCTION art_quick_imt_conflict_trampoline
    PUSH ecx
    movl 8(%esp), %eax            // load caller Method*
    movl METHOD_DEX_CACHE_METHODS_OFFSET(%eax), %eax  // load dex_cache_resolved_methods
    movd %xmm0, %ecx              // get target method index stored in xmm0
    movl OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4), %eax  // load the target method
    POP ecx
    jmp SYMBOL(art_quick_invoke_interface_trampoline)
END_FUNCTION art_quick_imt_conflict_trampoline

DEFINE_FUNCTION art_quick_resolution_trampoline
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    movl %esp, %edi
    PUSH EDI                      // pass SP. do not just PUSH ESP; that messes up unwinding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass method
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artQuickResolutionTrampoline) // (Method* called, receiver, Thread*, SP)
    movl %eax, %edi               // remember code pointer in EDI
    addl LITERAL(16), %esp        // pop arguments
    test %eax, %eax               // if code pointer is NULL goto deliver pending exception
    jz 1f
    POP eax                       // called method
    POP ecx                       // restore args
    POP edx
    POP ebx
    POP ebp                       // restore callee saves except EDI
    POP esi
    xchgl 0(%esp),%edi            // restore EDI and place code pointer as only value on stack
    ret                           // tail call into method
1:
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_resolution_trampoline

DEFINE_FUNCTION_NO_HIDE art_quick_generic_jni_trampoline
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    // This also stores the native ArtMethod reference at the bottom of the stack.

    movl %esp, %ebp                 // save SP at callee-save frame
    CFI_DEF_CFA_REGISTER(ebp)
    subl LITERAL(5120), %esp
    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //  (esp)    4(esp)   <= C calling convention
    //  fs:...    ebp     <= where they are
    // Also: PLT, so need GOT in ebx.

    subl LITERAL(8), %esp         // Padding for 16B alignment.
    pushl %ebp                    // Pass SP (to ArtMethod).
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    SETUP_GOT_NOSAVE              // Clobbers ebx.
    call SYMBOL(artQuickGenericJniTrampoline)  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // eax: pointer to native code, 0 on error.
    // edx: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    test %eax, %eax
    jz .Lentry_error

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

    // Tear down the alloca.
    movl %ebp, %esp
    CFI_DEF_CFA_REGISTER(esp)

    // Pending exceptions possible.
    mov %fs:THREAD_EXCEPTION_OFFSET, %ebx
    testl %ebx, %ebx
    jnz .Lexception_in_native

    // Tear down the callee-save frame.
    addl LITERAL(4), %esp     // Remove padding
    CFI_ADJUST_CFA_OFFSET(-4)
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
.Lentry_error:
    movl %ebp, %esp
    CFI_DEF_CFA_REGISTER(esp)
.Lexception_in_native:
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_generic_jni_trampoline

DEFINE_FUNCTION_NO_HIDE art_quick_to_interpreter_bridge
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME   // save frame
    mov %esp, %edx                // remember SP
    PUSH eax                      // alignment padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass  method
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artQuickToInterpreterBridge)  // (method, Thread*, SP)
    movd %eax, %xmm0              // place return value also into floating point return value
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
DEFINE_FUNCTION art_quick_instrumentation_entry
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    movl  %esp, %edx              // Save SP.
    PUSH eax                      // Save eax which will be clobbered by the callee-save method.
    subl LITERAL(8), %esp         // Align stack.
    CFI_ADJUST_CFA_OFFSET(8)
    pushl 40(%esp)                // Pass LR.
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // Pass SP.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // Pass receiver.
    PUSH eax                      // Pass Method*.
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artInstrumentationMethodEntryFromCode) // (Method*, Object*, Thread*, SP, LR)
    addl LITERAL(28), %esp        // Pop arguments upto saved Method*.
    movl 28(%esp), %edi           // Restore edi.
    movl %eax, 28(%esp)           // Place code* over edi, just under return pc.
    movl SYMBOL(art_quick_instrumentation_exit)@GOT(%ebx), %ebx
    movl %ebx, 32(%esp)
                                  // Place instrumentation exit as return pc.
    movl (%esp), %eax             // Restore eax.
    movl 8(%esp), %ecx            // Restore ecx.
    movl 12(%esp), %edx           // Restore edx.
    movl 16(%esp), %ebx           // Restore ebx.
    movl 20(%esp), %ebp           // Restore ebp.
    movl 24(%esp), %esi           // Restore esi.
    addl LITERAL(28), %esp        // Wind stack back upto code*.
    ret                           // Call method (and pop).
END_FUNCTION art_quick_instrumentation_entry

DEFINE_FUNCTION art_quick_instrumentation_exit
    pushl LITERAL(0)              // Push a fake return PC as there will be none on the stack.
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME
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
    SETUP_GOT_NOSAVE              // clobbers EBX
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
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    addl LITERAL(4), %esp         // Remove fake return pc.
    jmp   *%ecx                   // Return.
END_FUNCTION art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
DEFINE_FUNCTION art_quick_deoptimize
    pushl %ebx                    // Fake that we were called.
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    mov  %esp, %ecx               // Remember SP.
    subl LITERAL(8), %esp         // Align stack.
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH ecx                      // Pass SP.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    SETUP_GOT_NOSAVE              // clobbers EBX
    call SYMBOL(artDeoptimize)  // artDeoptimize(Thread*, SP)
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
    mov STRING_COUNT_OFFSET(%eax), %edx
    mov STRING_COUNT_OFFSET(%ecx), %ebx
    mov STRING_VALUE_OFFSET(%eax), %esi
    mov STRING_VALUE_OFFSET(%ecx), %edi
    mov STRING_OFFSET_OFFSET(%eax), %eax
    mov STRING_OFFSET_OFFSET(%ecx), %ecx
    /* Build pointers to the start of string data */
    lea  STRING_DATA_OFFSET(%esi, %eax, 2), %esi
    lea  STRING_DATA_OFFSET(%edi, %ecx, 2), %edi
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
    SETUP_GOT_NOSAVE                // sets %ebx for call into PLT
    movl LITERAL(1), %ecx
    pushl %ecx                      // second arg to longjmp (1)
    pushl %eax                      // first arg to longjmp (jmp_buf)
    call PLT_SYMBOL(longjmp)
    int3                            // won't get here.
END_FUNCTION art_nested_signal_return

    // TODO: implement these!
UNIMPLEMENTED art_quick_memcmp16
