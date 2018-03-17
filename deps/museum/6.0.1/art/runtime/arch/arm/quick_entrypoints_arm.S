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

#include "asm_support_arm.S"

#include "arch/quick_alloc_entrypoints.S"

    /* Deliver the given exception */
    .extern artDeliverExceptionFromCode
    /* Deliver an exception pending on a thread */
    .extern artDeliverPendingException

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVE_FRAME rTemp1, rTemp2
    push {r4-r11, lr}                             @ 9 words (36 bytes) of callee saves.
    .cfi_adjust_cfa_offset 36
    .cfi_rel_offset r4, 0
    .cfi_rel_offset r5, 4
    .cfi_rel_offset r6, 8
    .cfi_rel_offset r7, 12
    .cfi_rel_offset r8, 16
    .cfi_rel_offset r9, 20
    .cfi_rel_offset r10, 24
    .cfi_rel_offset r11, 28
    .cfi_rel_offset lr, 32
    vpush {s16-s31}                               @ 16 words (64 bytes) of floats.
    .cfi_adjust_cfa_offset 64
    sub sp, #12                                   @ 3 words of space, bottom word will hold Method*
    .cfi_adjust_cfa_offset 12
    RUNTIME_CURRENT1 \rTemp1, \rTemp2             @ Load Runtime::Current into rTemp1.
    THIS_LOAD_REQUIRES_READ_BARRIER
    ldr \rTemp1, [\rTemp1, #RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET] @ rTemp1 is kSaveAll Method*.
    str \rTemp1, [sp, #0]                         @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.

     // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVE != 36 + 64 + 12)
#error "SAVE_ALL_CALLEE_SAVE_FRAME(ARM) size not as expected."
#endif
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly).
     */
.macro SETUP_REFS_ONLY_CALLEE_SAVE_FRAME rTemp1, rTemp2
    push {r5-r8, r10-r11, lr}                     @ 7 words of callee saves
    .cfi_adjust_cfa_offset 28
    .cfi_rel_offset r5, 0
    .cfi_rel_offset r6, 4
    .cfi_rel_offset r7, 8
    .cfi_rel_offset r8, 12
    .cfi_rel_offset r10, 16
    .cfi_rel_offset r11, 20
    .cfi_rel_offset lr, 24
    sub sp, #4                                    @ bottom word will hold Method*
    .cfi_adjust_cfa_offset 4
    RUNTIME_CURRENT2 \rTemp1, \rTemp2             @ Load Runtime::Current into rTemp1.
    THIS_LOAD_REQUIRES_READ_BARRIER
    ldr \rTemp1, [\rTemp1, #RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET] @ rTemp1 is kRefsOnly Method*.
    str \rTemp1, [sp, #0]                         @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE != 28 + 4)
#error "REFS_ONLY_CALLEE_SAVE_FRAME(ARM) size not as expected."
#endif
.endm

.macro RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    add sp, #4               @ bottom word holds Method*
    .cfi_adjust_cfa_offset -4
    pop {r5-r8, r10-r11, lr} @ 7 words of callee saves
    .cfi_restore r5
    .cfi_restore r6
    .cfi_restore r7
    .cfi_restore r8
    .cfi_restore r10
    .cfi_restore r11
    .cfi_restore lr
    .cfi_adjust_cfa_offset -28
.endm

.macro RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    bx  lr                   @ return
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs).
     */
.macro SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_REGISTERS_ONLY
    push {r1-r3, r5-r8, r10-r11, lr}   @ 10 words of callee saves and args.
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r1, 0
    .cfi_rel_offset r2, 4
    .cfi_rel_offset r3, 8
    .cfi_rel_offset r5, 12
    .cfi_rel_offset r6, 16
    .cfi_rel_offset r7, 20
    .cfi_rel_offset r8, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset r11, 32
    .cfi_rel_offset lr, 36
    vpush {s0-s15}                     @ 16 words of float args.
    .cfi_adjust_cfa_offset 64
    sub sp, #8                         @ 2 words of space, bottom word will hold Method*
    .cfi_adjust_cfa_offset 8
    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE != 40 + 64 + 8)
#error "REFS_AND_ARGS_CALLEE_SAVE_FRAME(ARM) size not as expected."
#endif
.endm

.macro SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME rTemp1, rTemp2
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_REGISTERS_ONLY
    RUNTIME_CURRENT3 \rTemp1, \rTemp2  @ Load Runtime::Current into rTemp1.
    THIS_LOAD_REQUIRES_READ_BARRIER
     @ rTemp1 is kRefsAndArgs Method*.
    ldr \rTemp1, [\rTemp1, #RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET]
    str \rTemp1, [sp, #0]                         @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.
.endm

.macro SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_R0
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_REGISTERS_ONLY
    str r0, [sp, #0]                   @ Store ArtMethod* to bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.
.endm

.macro RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    add  sp, #8                      @ rewind sp
    .cfi_adjust_cfa_offset -8
    vpop {s0-s15}
    .cfi_adjust_cfa_offset -64
    pop {r1-r3, r5-r8, r10-r11, lr}  @ 10 words of callee saves
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r5
    .cfi_restore r6
    .cfi_restore r7
    .cfi_restore r8
    .cfi_restore r10
    .cfi_restore r11
    .cfi_restore lr
    .cfi_adjust_cfa_offset -40
.endm


.macro RETURN_IF_RESULT_IS_ZERO
    cbnz   r0, 1f              @ result non-zero branch over
    bx     lr                  @ return
1:
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO
    cbz    r0, 1f              @ result zero branch over
    bx     lr                  @ return
1:
.endm

    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_
     */
.macro DELIVER_PENDING_EXCEPTION
    .fnend
    .fnstart
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME r0, r1    @ save callee saves for throw
    mov    r0, r9                              @ pass Thread::Current
    b      artDeliverPendingExceptionFromCode  @ artDeliverPendingExceptionFromCode(Thread*)
.endm

.macro NO_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  r0, r1 // save all registers as basis for long jump context
    mov r0, r9                      @ pass Thread::Current
    b   \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro ONE_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME r1, r2  // save all registers as basis for long jump context
    mov r1, r9                      @ pass Thread::Current
    b   \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro TWO_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  r2, r3  // save all registers as basis for long jump context
    mov r2, r9                      @ pass Thread::Current
    b   \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro  RETURN_OR_DELIVER_PENDING_EXCEPTION_REG reg
    ldr \reg, [r9, #THREAD_EXCEPTION_OFFSET]   // Get exception field.
    cbnz \reg, 1f
    bx lr
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro  RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
    RETURN_OR_DELIVER_PENDING_EXCEPTION_REG r1
.endm

.macro RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
    RETURN_IF_RESULT_IS_NON_ZERO
    DELIVER_PENDING_EXCEPTION
.endm

// Macros taking opportunity of code similarities for downcalls with referrer for non-wide fields.
.macro  ONE_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r1, r2  @ save callee saves in case of GC
    ldr    r1, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    mov    r2, r9                        @ pass Thread::Current
    bl     \entrypoint                   @ (uint32_t field_idx, const Method* referrer, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    \return
END \name
.endm

.macro  TWO_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r2, r3  @ save callee saves in case of GC
    ldr    r2, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    mov    r3, r9                        @ pass Thread::Current
    bl     \entrypoint                   @ (field_idx, Object*, referrer, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    \return
END \name
.endm

.macro THREE_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r3, r12  @ save callee saves in case of GC
    ldr    r3, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    str    r9, [sp, #-16]!               @ expand the frame and pass Thread::Current
    .cfi_adjust_cfa_offset 16
    bl     \entrypoint                   @ (field_idx, Object*, new_val, referrer, Thread*)
    add    sp, #16                       @ release out args
    .cfi_adjust_cfa_offset -16
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME   @ TODO: we can clearly save an add here
    \return
END \name
.endm

    /*
     * Called by managed code, saves callee saves and then calls artThrowException
     * that will place a mock Method* at the bottom of the stack. Arg1 holds the exception.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_deliver_exception, artDeliverExceptionFromCode

    /*
     * Called by managed code to create and deliver a NullPointerException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_null_pointer_exception, artThrowNullPointerExceptionFromCode

    /*
     * Called by managed code to create and deliver an ArithmeticException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_div_zero, artThrowDivZeroFromCode

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException. Arg1 holds
     * index, arg2 holds limit.
     */
TWO_ARG_RUNTIME_EXCEPTION art_quick_throw_array_bounds, artThrowArrayBoundsFromCode

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_stack_overflow, artThrowStackOverflowFromCode

    /*
     * Called by managed code to create and deliver a NoSuchMethodError.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_throw_no_such_method, artThrowNoSuchMethodFromCode

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
     * If unsuccessful, the helper will return null/null. There will bea pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
.macro INVOKE_TRAMPOLINE c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME r2, r3  @ save callee saves in case allocation triggers GC
    ldr    r2, [sp, #FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE]  @ pass caller Method*
    mov    r3, r9                         @ pass Thread::Current
    mov    r12, sp
    str    r12, [sp, #-16]!               @ expand the frame and pass SP
    .cfi_adjust_cfa_offset 16
    bl     \cxx_name                      @ (method_idx, this, caller, Thread*, SP)
    add    sp, #16                        @ strip the extra frame
    .cfi_adjust_cfa_offset -16
    mov    r12, r1                        @ save Method*->code_
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    cbz    r0, 1f                         @ did we find the target? if not go to exception delivery
    bx     r12                            @ tail call to target
1:
    DELIVER_PENDING_EXCEPTION
END \c_name
.endm

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline, artInvokeInterfaceTrampoline
INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck

    /*
     * Quick invocation stub internal.
     * On entry:
     *   r0 = method pointer
     *   r1 = argument array or null for no argument methods
     *   r2 = size of argument array in bytes
     *   r3 = (managed) thread pointer
     *   [sp] = JValue* result
     *   [sp + 4] = result_in_float
     *   [sp + 8] = core register argument array
     *   [sp + 12] = fp register argument array
     *  +-------------------------+
     *  | uint32_t* fp_reg_args   |
     *  | uint32_t* core_reg_args |
     *  |   result_in_float       | <- Caller frame
     *  |   Jvalue* result        |
     *  +-------------------------+
     *  |          lr             |
     *  |          r11            |
     *  |          r9             |
     *  |          r4             | <- r11
     *  +-------------------------+
     *  | uint32_t out[n-1]       |
     *  |    :      :             |        Outs
     *  | uint32_t out[0]         |
     *  | StackRef<ArtMethod>     | <- SP  value=null
     *  +-------------------------+
     */
ENTRY art_quick_invoke_stub_internal
    push   {r4, r5, r6, r7, r8, r9, r10, r11, lr}               @ spill regs
    .cfi_adjust_cfa_offset 16
    .cfi_rel_offset r4, 0
    .cfi_rel_offset r5, 4
    .cfi_rel_offset r6, 8
    .cfi_rel_offset r7, 12
    .cfi_rel_offset r8, 16
    .cfi_rel_offset r9, 20
    .cfi_rel_offset r10, 24
    .cfi_rel_offset r11, 28
    .cfi_rel_offset lr, 32
    mov    r11, sp                         @ save the stack pointer
    .cfi_def_cfa_register r11

    mov    r9, r3                          @ move managed thread pointer into r9

    add    r4, r2, #4                      @ create space for method pointer in frame
    sub    r4, sp, r4                      @ reserve & align *stack* to 16 bytes: native calling
    and    r4, #0xFFFFFFF0                 @ convention only aligns to 8B, so we have to ensure ART
    mov    sp, r4                          @ 16B alignment ourselves.

    mov    r4, r0                          @ save method*
    add    r0, sp, #4                      @ pass stack pointer + method ptr as dest for memcpy
    bl     memcpy                          @ memcpy (dest, src, bytes)
    mov    ip, #0                          @ set ip to 0
    str    ip, [sp]                        @ store null for method* at bottom of frame

    ldr    ip, [r11, #48]                  @ load fp register argument array pointer
    vldm   ip, {s0-s15}                    @ copy s0 - s15

    ldr    ip, [r11, #44]                  @ load core register argument array pointer
    mov    r0, r4                          @ restore method*
    add    ip, ip, #4                      @ skip r0
    ldm    ip, {r1-r3}                     @ copy r1 - r3

#ifdef ARM_R4_SUSPEND_FLAG
    mov    r4, #SUSPEND_CHECK_INTERVAL     @ reset r4 to suspend check interval
#endif

    ldr    ip, [r0, #ART_METHOD_QUICK_CODE_OFFSET_32]  @ get pointer to the code
    blx    ip                              @ call the method

    mov    sp, r11                         @ restore the stack pointer
    .cfi_def_cfa_register sp

    ldr    r4, [sp, #40]                   @ load result_is_float
    ldr    r9, [sp, #36]                   @ load the result pointer
    cmp    r4, #0
    ite    eq
    strdeq r0, [r9]                        @ store r0/r1 into result pointer
    vstrne d0, [r9]                        @ store s0-s1/d0 into result pointer

    pop    {r4, r5, r6, r7, r8, r9, r10, r11, pc}               @ restore spill regs
END art_quick_invoke_stub_internal

    /*
     * On entry r0 is uint32_t* gprs_ and r1 is uint32_t* fprs_
     */
ARM_ENTRY art_quick_do_long_jump
    vldm r1, {s0-s31}     @ load all fprs from argument fprs_
    ldr  r2, [r0, #60]    @ r2 = r15 (PC from gprs_ 60=4*15)
    ldr  r14, [r0, #56]   @ (LR from gprs_ 56=4*14)
    add  r0, r0, #12      @ increment r0 to skip gprs_[0..2] 12=4*3
    ldm  r0, {r3-r13}     @ load remaining gprs from argument gprs_
    mov  r0, #0           @ clear result registers r0 and r1
    mov  r1, #0
    bx   r2               @ do long jump
END art_quick_do_long_jump

    /*
     * Entry from managed code that calls artHandleFillArrayDataFromCode and delivers exception on
     * failure.
     */
TWO_ARG_REF_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER

    /*
     * Entry from managed code that calls artLockObjectFromCode, may block for GC. r0 holds the
     * possibly null object to lock.
     */
    .extern artLockObjectFromCode
ENTRY art_quick_lock_object
    cbz    r0, .Lslow_lock
.Lretry_lock:
    ldr    r2, [r9, #THREAD_ID_OFFSET]
    ldrex  r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    mov    r3, r1
    and    r3, #LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED  @ zero the read barrier bits
    cbnz   r3, .Lnot_unlocked         @ already thin locked
    @ unlocked case - r1: original lock word that's zero except for the read barrier bits.
    orr    r2, r1, r2                 @ r2 holds thread id with count of 0 with preserved read barrier bits
    strex  r3, r2, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    cbnz   r3, .Llock_strex_fail      @ store failed, retry
    dmb    ish                        @ full (LoadLoad|LoadStore) memory barrier
    bx lr
.Lnot_unlocked:  @ r1: original lock word, r2: thread_id with count of 0 and zero read barrier bits
    lsr    r3, r1, LOCK_WORD_STATE_SHIFT
    cbnz   r3, .Lslow_lock            @ if either of the top two bits are set, go slow path
    eor    r2, r1, r2                 @ lock_word.ThreadId() ^ self->ThreadId()
    uxth   r2, r2                     @ zero top 16 bits
    cbnz   r2, .Lslow_lock            @ lock word and self thread id's match -> recursive lock
                                      @ else contention, go to slow path
    mov    r3, r1                     @ copy the lock word to check count overflow.
    and    r3, #LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED  @ zero the read barrier bits.
    add    r2, r3, #LOCK_WORD_THIN_LOCK_COUNT_ONE  @ increment count in lock word placing in r2 to check overflow
    lsr    r3, r2, LOCK_WORD_READ_BARRIER_STATE_SHIFT  @ if either of the upper two bits (28-29) are set, we overflowed.
    cbnz   r3, .Lslow_lock            @ if we overflow the count go slow path
    add    r2, r1, #LOCK_WORD_THIN_LOCK_COUNT_ONE  @ increment count for real
    strex  r3, r2, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET] @ strex necessary for read barrier bits
    cbnz   r3, .Llock_strex_fail      @ strex failed, retry
    bx lr
.Llock_strex_fail:
    b      .Lretry_lock               @ retry
.Lslow_lock:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r1, r2  @ save callee saves in case we block
    mov    r1, r9                     @ pass Thread::Current
    bl     artLockObjectFromCode      @ (Object* obj, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_lock_object

    /*
     * Entry from managed code that calls artUnlockObjectFromCode and delivers exception on failure.
     * r0 holds the possibly null object to lock.
     */
    .extern artUnlockObjectFromCode
ENTRY art_quick_unlock_object
    cbz    r0, .Lslow_unlock
.Lretry_unlock:
#ifndef USE_READ_BARRIER
    ldr    r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
#else
    ldrex  r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]  @ Need to use atomic instructions for read barrier
#endif
    lsr    r2, r1, #LOCK_WORD_STATE_SHIFT
    cbnz   r2, .Lslow_unlock          @ if either of the top two bits are set, go slow path
    ldr    r2, [r9, #THREAD_ID_OFFSET]
    mov    r3, r1                     @ copy lock word to check thread id equality
    and    r3, #LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED  @ zero the read barrier bits
    eor    r3, r3, r2                 @ lock_word.ThreadId() ^ self->ThreadId()
    uxth   r3, r3                     @ zero top 16 bits
    cbnz   r3, .Lslow_unlock          @ do lock word and self thread id's match?
    mov    r3, r1                     @ copy lock word to detect transition to unlocked
    and    r3, #LOCK_WORD_READ_BARRIER_STATE_MASK_TOGGLED  @ zero the read barrier bits
    cmp    r3, #LOCK_WORD_THIN_LOCK_COUNT_ONE
    bpl    .Lrecursive_thin_unlock
    @ transition to unlocked
    mov    r3, r1
    and    r3, #LOCK_WORD_READ_BARRIER_STATE_MASK  @ r3: zero except for the preserved read barrier bits
    dmb    ish                        @ full (LoadStore|StoreStore) memory barrier
#ifndef USE_READ_BARRIER
    str    r3, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
#else
    strex  r2, r3, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]  @ strex necessary for read barrier bits
    cbnz   r2, .Lunlock_strex_fail    @ store failed, retry
#endif
    bx     lr
.Lrecursive_thin_unlock:  @ r1: original lock word
    sub    r1, r1, #LOCK_WORD_THIN_LOCK_COUNT_ONE  @ decrement count
#ifndef USE_READ_BARRIER
    str    r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
#else
    strex  r2, r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]  @ strex necessary for read barrier bits
    cbnz   r2, .Lunlock_strex_fail    @ store failed, retry
#endif
    bx     lr
.Lunlock_strex_fail:
    b      .Lretry_unlock             @ retry
.Lslow_unlock:
    @ save callee saves in case exception allocation triggers GC
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r1, r2
    mov    r1, r9                     @ pass Thread::Current
    bl     artUnlockObjectFromCode    @ (Object* obj, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_unlock_object

    /*
     * Entry from managed code that calls artIsAssignableFromCode and on failure calls
     * artThrowClassCastException.
     */
    .extern artThrowClassCastException
ENTRY art_quick_check_cast
    push {r0-r1, lr}                    @ save arguments, link register and pad
    .cfi_adjust_cfa_offset 12
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset lr, 8
    sub sp, #4
    .cfi_adjust_cfa_offset 4
    bl artIsAssignableFromCode
    cbz    r0, .Lthrow_class_cast_exception
    add sp, #4
    .cfi_adjust_cfa_offset -4
    pop {r0-r1, pc}
    .cfi_adjust_cfa_offset 4        @ Reset unwind info so following code unwinds.
.Lthrow_class_cast_exception:
    add sp, #4
    .cfi_adjust_cfa_offset -4
    pop {r0-r1, lr}
    .cfi_adjust_cfa_offset -12
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore lr
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME r2, r3  // save all registers as basis for long jump context
    mov r2, r9                      @ pass Thread::Current
    b   artThrowClassCastException  @ (Class*, Class*, Thread*)
    bkpt
END art_quick_check_cast

    /*
     * Entry from managed code for array put operations of objects where the value being stored
     * needs to be checked for compatibility.
     * r0 = array, r1 = index, r2 = value
     */
ENTRY art_quick_aput_obj_with_null_and_bound_check
    tst r0, r0
    bne art_quick_aput_obj_with_bound_check
    b art_quick_throw_null_pointer_exception
END art_quick_aput_obj_with_null_and_bound_check

    .hidden art_quick_aput_obj_with_bound_check
ENTRY art_quick_aput_obj_with_bound_check
    ldr r3, [r0, #MIRROR_ARRAY_LENGTH_OFFSET]
    cmp r3, r1
    bhi art_quick_aput_obj
    mov r0, r1
    mov r1, r3
    b art_quick_throw_array_bounds
END art_quick_aput_obj_with_bound_check

    .hidden art_quick_aput_obj
ENTRY art_quick_aput_obj
    cbz r2, .Ldo_aput_null
    ldr r3, [r0, #MIRROR_OBJECT_CLASS_OFFSET]
    ldr ip, [r2, #MIRROR_OBJECT_CLASS_OFFSET]
    ldr r3, [r3, #MIRROR_CLASS_COMPONENT_TYPE_OFFSET]
    cmp r3, ip  @ value's type == array's component type - trivial assignability
    bne .Lcheck_assignability
.Ldo_aput:
    add r3, r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
    str r2, [r3, r1, lsl #2]
    ldr r3, [r9, #THREAD_CARD_TABLE_OFFSET]
    lsr r0, r0, #7
    strb r3, [r3, r0]
    blx lr
.Ldo_aput_null:
    add r3, r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
    str r2, [r3, r1, lsl #2]
    blx lr
.Lcheck_assignability:
    push {r0-r2, lr}             @ save arguments
    .cfi_adjust_cfa_offset 16
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset lr, 12
    mov r1, ip
    mov r0, r3
    bl artIsAssignableFromCode
    cbz r0, .Lthrow_array_store_exception
    pop {r0-r2, lr}
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore lr
    .cfi_adjust_cfa_offset -16
    add r3, r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
    str r2, [r3, r1, lsl #2]
    ldr r3, [r9, #THREAD_CARD_TABLE_OFFSET]
    lsr r0, r0, #7
    strb r3, [r3, r0]
    blx lr
.Lthrow_array_store_exception:
    pop {r0-r2, lr}
    /* No need to repeat restore cfi directives, the ones above apply here. */
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME r3, ip
    mov r1, r2
    mov r2, r9                     @ pass Thread::Current
    b artThrowArrayStoreException  @ (Class*, Class*, Thread*)
    bkpt                           @ unreached
END art_quick_aput_obj

// Macro to facilitate adding new allocation entrypoints.
.macro ONE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  r1, r2  @ save callee saves in case of GC
    mov    r1, r9                     @ pass Thread::Current
    bl     \entrypoint     @ (uint32_t type_idx, Method* method, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  r2, r3  @ save callee saves in case of GC
    mov    r2, r9                     @ pass Thread::Current
    bl     \entrypoint     @ (uint32_t type_idx, Method* method, Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    \return
END \name
.endm

// Macro to facilitate adding new array allocation entrypoints.
.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  r3, r12  @ save callee saves in case of GC
    mov    r3, r9                     @ pass Thread::Current
    @ (uint32_t type_idx, Method* method, int32_t component_count, Thread*)
    bl     \entrypoint
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro FOUR_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  r3, r12  @ save callee saves in case of GC
    str    r9, [sp, #-16]!            @ expand the frame and pass Thread::Current
    .pad #16
    .cfi_adjust_cfa_offset 16
    bl     \entrypoint
    add    sp, #16                    @ strip the extra frame
    .cfi_adjust_cfa_offset -16
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    \return
END \name
.endm

ONE_ARG_DOWNCALL art_quick_initialize_static_storage, artInitializeStaticStorageFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
ONE_ARG_DOWNCALL art_quick_initialize_type, artInitializeTypeFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
ONE_ARG_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER

    /*
     * Called by managed code to resolve a static field and load a non-wide value.
     */
ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
    /*
     * Called by managed code to resolve a static field and load a 64-bit primitive value.
     */
    .extern artGet64StaticFromCode
ENTRY art_quick_get64_static
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r2, r3  @ save callee saves in case of GC
    ldr    r1, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    mov    r2, r9                        @ pass Thread::Current
    bl     artGet64StaticFromCode        @ (uint32_t field_idx, const Method* referrer, Thread*)
    ldr    r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    cbnz   r2, 1f                        @ success if no exception pending
    bx     lr                            @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_get64_static

    /*
     * Called by managed code to resolve an instance field and load a non-wide value.
     */
TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
    /*
     * Called by managed code to resolve an instance field and load a 64-bit primitive value.
     */
    .extern artGet64InstanceFromCode
ENTRY art_quick_get64_instance
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME  r2, r3  @ save callee saves in case of GC
    ldr    r2, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    mov    r3, r9                        @ pass Thread::Current
    bl     artGet64InstanceFromCode      @ (field_idx, Object*, referrer, Thread*)
    ldr    r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    cbnz   r2, 1f                        @ success if no exception pending
    bx     lr                            @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_get64_instance

    /*
     * Called by managed code to resolve a static field and store a non-wide value.
     */
TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
    /*
     * Called by managed code to resolve a static field and store a 64-bit primitive value.
     * On entry r0 holds field index, r1:r2 hold new_val
     */
    .extern artSet64StaticFromCode
ENTRY art_quick_set64_static
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r3, r12   @ save callee saves in case of GC
    mov    r3, r2                        @ pass one half of wide argument
    mov    r2, r1                        @ pass other half of wide argument
    ldr    r1, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    str    r9, [sp, #-16]!               @ expand the frame and pass Thread::Current
    .cfi_adjust_cfa_offset 16
    bl     artSet64StaticFromCode        @ (field_idx, referrer, new_val, Thread*)
    add    sp, #16                       @ release out args
    .cfi_adjust_cfa_offset -16
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  @ TODO: we can clearly save an add here
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_set64_static

    /*
     * Called by managed code to resolve an instance field and store a non-wide value.
     */
THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
    /*
     * Called by managed code to resolve an instance field and store a 64-bit primitive value.
     */
    .extern artSet64InstanceFromCode
ENTRY art_quick_set64_instance
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r12, lr  @ save callee saves in case of GC
    ldr    r12, [sp, #FRAME_SIZE_REFS_ONLY_CALLEE_SAVE]  @ pass referrer
    str    r9, [sp, #-12]!               @ expand the frame and pass Thread::Current
    .cfi_adjust_cfa_offset 12
    str    r12, [sp, #-4]!               @ expand the frame and pass the referrer
    .cfi_adjust_cfa_offset 4
    bl     artSet64InstanceFromCode      @ (field_idx, Object*, new_val, Method* referrer, Thread*)
    add    sp, #16                       @ release out args
    .cfi_adjust_cfa_offset -16
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME  @ TODO: we can clearly save an add here
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_set64_instance

    /*
     * Entry from managed code to resolve a string, this stub will allocate a String and deliver an
     * exception on error. On success the String is returned. R0 holds the string index. The fast
     * path check for hit in strings cache has already been performed.
     */
ONE_ARG_DOWNCALL art_quick_resolve_string, artResolveStringFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER

// Generate the allocation entrypoints for each allocator.
GENERATE_ALL_ALLOC_ENTRYPOINTS

    /*
     * Called by managed code when the value in rSUSPEND has been decremented to 0.
     */
    .extern artTestSuspendFromCode
ENTRY art_quick_test_suspend
#ifdef ARM_R4_SUSPEND_FLAG
    ldrh   r0, [rSELF, #THREAD_FLAGS_OFFSET]
    mov    rSUSPEND, #SUSPEND_CHECK_INTERVAL  @ reset rSUSPEND to SUSPEND_CHECK_INTERVAL
    cbnz   r0, 1f                             @ check Thread::Current()->suspend_count_ == 0
    bx     lr                                 @ return if suspend_count_ == 0
1:
#endif
    mov    r0, rSELF
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r1, r2   @ save callee saves for GC stack crawl
    @ TODO: save FPRs to enable access in the debugger?
    bl     artTestSuspendFromCode             @ (Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
END art_quick_test_suspend

ENTRY art_quick_implicit_suspend
    mov    r0, rSELF
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r1, r2   @ save callee saves for stack crawl
    bl     artTestSuspendFromCode             @ (Thread*)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
END art_quick_implicit_suspend

    /*
     * Called by managed code that is attempting to call a method on a proxy class. On entry
     * r0 holds the proxy method and r1 holds the receiver; r2 and r3 may contain arguments. The
     * frame size of the invoked proxy method agrees with a ref and args callee save frame.
     */
     .extern artQuickProxyInvokeHandler
ENTRY art_quick_proxy_invoke_handler
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_R0
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    blx     artQuickProxyInvokeHandler  @ (Method* proxy method, receiver, Thread*, SP)
    ldr     r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    // Tear down the callee-save frame. Skip arg registers.
    add     sp, #(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE - FRAME_SIZE_REFS_ONLY_CALLEE_SAVE)
    .cfi_adjust_cfa_offset -(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE - FRAME_SIZE_REFS_ONLY_CALLEE_SAVE)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    cbnz    r2, 1f                 @ success if no exception is pending
    vmov    d0, r0, r1             @ store into fpr, for when it's a fpr return...
    bx      lr                     @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict. r12 is a hidden argument that holds the target method's
     * dex method index.
     */
ENTRY art_quick_imt_conflict_trampoline
    ldr    r0, [sp, #0]            @ load caller Method*
    ldr    r0, [r0, #ART_METHOD_DEX_CACHE_METHODS_OFFSET]  @ load dex_cache_resolved_methods
    add    r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET  @ get starting address of data
    ldr    r0, [r0, r12, lsl 2]    @ load the target method
    b art_quick_invoke_interface_trampoline
END art_quick_imt_conflict_trampoline

    .extern artQuickResolutionTrampoline
ENTRY art_quick_resolution_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME r2, r3
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    blx     artQuickResolutionTrampoline  @ (Method* called, receiver, Thread*, SP)
    cbz     r0, 1f                 @ is code pointer null? goto exception
    mov     r12, r0
    ldr  r0, [sp, #0]              @ load resolved method in r0
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    bx      r12                    @ tail-call into actual code
1:
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_resolution_trampoline

    /*
     * Called to do a generic JNI down-call
     */
ENTRY art_quick_generic_jni_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_R0

    // Save rSELF
    mov r11, rSELF
    // Save SP , so we can have static CFI info. r10 is saved in ref_and_args.
    mov r10, sp
    .cfi_def_cfa_register r10

    sub sp, sp, #5120

    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //    r0      r1   <= C calling convention
    //  rSELF     r10  <= where they are

    mov r0, rSELF   // Thread*
    mov r1, r10
    blx artQuickGenericJniTrampoline  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // r0: pointer to native code, 0 on error.
    // r1: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    cbz r0, .Lexception_in_native

    // Release part of the alloca.
    mov sp, r1

    // Save the code pointer
    mov r12, r0

    // Load parameters from frame into registers.
    pop {r0-r3}

    // Softfloat.
    // TODO: Change to hardfloat when supported.

    blx r12           // native call.

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*, result, result_f)
    //    r0      r2,r3    stack       <= C calling convention
    //    r11     r0,r1    r0,r1          <= where they are
    sub sp, sp, #8 // Stack alignment.

    push {r0-r1}
    mov r3, r1
    mov r2, r0
    mov r0, r11

    blx artQuickGenericJniEndTrampoline

    // Restore self pointer.
    mov r9, r11

    // Pending exceptions possible.
    ldr r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    cbnz r2, .Lexception_in_native

    // Tear down the alloca.
    mov sp, r10
    .cfi_def_cfa_register sp

    // Tear down the callee-save frame. Skip arg registers.
    add     sp, #FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE-FRAME_SIZE_REFS_ONLY_CALLEE_SAVE
    .cfi_adjust_cfa_offset -(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE-FRAME_SIZE_REFS_ONLY_CALLEE_SAVE)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME

    // store into fpr, for when it's a fpr return...
    vmov d0, r0, r1
    bx lr      // ret
    // Undo the unwinding information from above since it doesn't apply below.
    .cfi_def_cfa_register r10
    .cfi_adjust_cfa_offset FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE-FRAME_SIZE_REFS_ONLY_CALLEE_SAVE

.Lexception_in_native:
    ldr sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]
    .cfi_def_cfa_register sp
    # This will create a new save-all frame, required by the runtime.
    DELIVER_PENDING_EXCEPTION
END art_quick_generic_jni_trampoline

    .extern artQuickToInterpreterBridge
ENTRY art_quick_to_interpreter_bridge
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME r1, r2
    mov     r1, r9                 @ pass Thread::Current
    mov     r2, sp                 @ pass SP
    blx     artQuickToInterpreterBridge    @ (Method* method, Thread*, SP)
    ldr     r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    // Tear down the callee-save frame. Skip arg registers.
    add     sp, #(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE - FRAME_SIZE_REFS_ONLY_CALLEE_SAVE)
    .cfi_adjust_cfa_offset -(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE - FRAME_SIZE_REFS_ONLY_CALLEE_SAVE)
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    cbnz    r2, 1f                 @ success if no exception is pending
    vmov    d0, r0, r1             @ store into fpr, for when it's a fpr return...
    bx      lr                     @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
    .extern artInstrumentationMethodEntryFromCode
    .extern artInstrumentationMethodExitFromCode
ENTRY art_quick_instrumentation_entry
    @ Make stack crawlable and clobber r2 and r3 (post saving)
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME r2, r3
    @ preserve r0 (not normally an arg) knowing there is a spare slot in kRefsAndArgs.
    str   r0, [sp, #4]
    mov   r2, r9         @ pass Thread::Current
    mov   r3, lr         @ pass LR
    blx   artInstrumentationMethodEntryFromCode  @ (Method*, Object*, Thread*, LR)
    mov   r12, r0        @ r12 holds reference to code
    ldr   r0, [sp, #4]   @ restore r0
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    blx   r12            @ call method with lr set to art_quick_instrumentation_exit
@ Deliberate fall-through into art_quick_instrumentation_exit.
    .type art_quick_instrumentation_exit, #function
    .global art_quick_instrumentation_exit
art_quick_instrumentation_exit:
    mov   lr, #0         @ link register is to here, so clobber with 0 for later checks
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME r2, r3  @ set up frame knowing r2 and r3 must be dead on exit
    mov   r12, sp        @ remember bottom of caller's frame
    push  {r0-r1}        @ save return value
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    vpush {d0}           @ save fp return value
    .cfi_adjust_cfa_offset 8
    sub   sp, #8         @ space for return value argument. Note: AAPCS stack alignment is 8B, no
                         @ need to align by 16.
    .cfi_adjust_cfa_offset 8
    vstr  d0, [sp]       @ d0 -> [sp] for fpr_res
    mov   r2, r0         @ pass return value as gpr_res
    mov   r3, r1
    mov   r0, r9         @ pass Thread::Current
    mov   r1, r12        @ pass SP
    blx   artInstrumentationMethodExitFromCode  @ (Thread*, SP, gpr_res, fpr_res)
    add   sp, #8
    .cfi_adjust_cfa_offset -8

    mov   r2, r0         @ link register saved by instrumentation
    mov   lr, r1         @ r1 is holding link register if we're to bounce to deoptimize
    vpop  {d0}           @ restore fp return value
    .cfi_adjust_cfa_offset -8
    pop   {r0, r1}       @ restore return value
    .cfi_adjust_cfa_offset -8
    .cfi_restore r0
    .cfi_restore r1
    add sp, #32          @ remove callee save frame
    .cfi_adjust_cfa_offset -32
    bx    r2             @ return
END art_quick_instrumentation_entry

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimize
ENTRY art_quick_deoptimize
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME r0, r1
    mov    r0, r9         @ Set up args.
    blx    artDeoptimize  @ artDeoptimize(Thread*)
END art_quick_deoptimize

    /*
     * Signed 64-bit integer multiply.
     *
     * Consider WXxYZ (r1r0 x r3r2) with a long multiply:
     *        WX
     *      x YZ
     *  --------
     *     ZW ZX
     *  YW YX
     *
     * The low word of the result holds ZX, the high word holds
     * (ZW+YX) + (the high overflow from ZX).  YW doesn't matter because
     * it doesn't fit in the low 64 bits.
     *
     * Unlike most ARM math operations, multiply instructions have
     * restrictions on using the same register more than once (Rd and Rm
     * cannot be the same).
     */
    /* mul-long vAA, vBB, vCC */
ENTRY art_quick_mul_long
    push    {r9 - r10}
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset r9, 0
    .cfi_rel_offset r10, 4
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    add     r10, r2, r10                @  r10<- r10 + low(ZxW + (YxX))
    mov     r0,r9
    mov     r1,r10
    pop     {r9 - r10}
    .cfi_adjust_cfa_offset -8
    .cfi_restore r9
    .cfi_restore r10
    bx      lr
END art_quick_mul_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   r0: low word
     *   r1: high word
     *   r2: shift count
     */
    /* shl-long vAA, vBB, vCC */
ARM_ENTRY art_quick_shl_long            @ ARM code as thumb code requires spills
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    mov     r1, r1, asl r2              @  r1<- r1 << r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @  r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r1, r0, asl ip              @  if r2 >= 32, r1<- r0 << (r2-32)
    mov     r0, r0, asl r2              @  r0<- r0 << r2
    bx      lr
END art_quick_shl_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   r0: low word
     *   r1: high word
     *   r2: shift count
     */
    /* shr-long vAA, vBB, vCC */
ARM_ENTRY art_quick_shr_long            @ ARM code as thumb code requires spills
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, asr ip              @  if r2 >= 32, r0<-r1 >> (r2-32)
    mov     r1, r1, asr r2              @  r1<- r1 >> r2
    bx      lr
END art_quick_shr_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   r0: low word
     *   r1: high word
     *   r2: shift count
     */
    /* ushr-long vAA, vBB, vCC */
ARM_ENTRY art_quick_ushr_long           @ ARM code as thumb code requires spills
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, lsr ip              @  if r2 >= 32, r0<-r1 >>> (r2-32)
    mov     r1, r1, lsr r2              @  r1<- r1 >>> r2
    bx      lr
END art_quick_ushr_long

    /*
     * String's indexOf.
     *
     * On entry:
     *    r0:   string object (known non-null)
     *    r1:   char to match (known <= 0xFFFF)
     *    r2:   Starting offset in string data
     */
ENTRY art_quick_indexof
    push {r4, r10-r11, lr} @ 4 words of callee saves
    .cfi_adjust_cfa_offset 16
    .cfi_rel_offset r4, 0
    .cfi_rel_offset r10, 4
    .cfi_rel_offset r11, 8
    .cfi_rel_offset lr, 12
    ldr   r3, [r0, #MIRROR_STRING_COUNT_OFFSET]
    add   r0, #MIRROR_STRING_VALUE_OFFSET

    /* Clamp start to [0..count] */
    cmp   r2, #0
    it    lt
    movlt r2, #0
    cmp   r2, r3
    it    gt
    movgt r2, r3

    /* Save a copy in r12 to later compute result */
    mov   r12, r0

    /* Build pointer to start of data to compare and pre-bias */
    add   r0, r0, r2, lsl #1
    sub   r0, #2

    /* Compute iteration count */
    sub   r2, r3, r2

    /*
     * At this point we have:
     *   r0: start of data to test
     *   r1: char to compare
     *   r2: iteration count
     *   r12: original start of string data
     *   r3, r4, r10, r11 available for loading string data
     */

    subs  r2, #4
    blt   .Lindexof_remainder

.Lindexof_loop4:
    ldrh  r3, [r0, #2]!
    ldrh  r4, [r0, #2]!
    ldrh  r10, [r0, #2]!
    ldrh  r11, [r0, #2]!
    cmp   r3, r1
    beq   .Lmatch_0
    cmp   r4, r1
    beq   .Lmatch_1
    cmp   r10, r1
    beq   .Lmatch_2
    cmp   r11, r1
    beq   .Lmatch_3
    subs  r2, #4
    bge   .Lindexof_loop4

.Lindexof_remainder:
    adds  r2, #4
    beq   .Lindexof_nomatch

.Lindexof_loop1:
    ldrh  r3, [r0, #2]!
    cmp   r3, r1
    beq   .Lmatch_3
    subs  r2, #1
    bne   .Lindexof_loop1

.Lindexof_nomatch:
    mov   r0, #-1
    pop {r4, r10-r11, pc}

.Lmatch_0:
    sub   r0, #6
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
.Lmatch_1:
    sub   r0, #4
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
.Lmatch_2:
    sub   r0, #2
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
.Lmatch_3:
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
END art_quick_indexof

   /*
     * String's compareTo.
     *
     * Requires rARG0/rARG1 to have been previously checked for null.  Will
     * return negative if this's string is < comp, 0 if they are the
     * same and positive if >.
     *
     * On entry:
     *    r0:   this object pointer
     *    r1:   comp object pointer
     *
     */
    .extern __memcmp16
ENTRY art_quick_string_compareto
    mov    r2, r0         @ this to r2, opening up r0 for return value
    sub    r0, r2, r1     @ Same?
    cbnz   r0,1f
    bx     lr
1:                        @ Same strings, return.

    push {r4, r7-r12, lr} @ 8 words - keep alignment
    .cfi_adjust_cfa_offset 32
    .cfi_rel_offset r4, 0
    .cfi_rel_offset r7, 4
    .cfi_rel_offset r8, 8
    .cfi_rel_offset r9, 12
    .cfi_rel_offset r10, 16
    .cfi_rel_offset r11, 20
    .cfi_rel_offset r12, 24
    .cfi_rel_offset lr, 28

    ldr    r7, [r2, #MIRROR_STRING_COUNT_OFFSET]
    ldr    r10, [r1, #MIRROR_STRING_COUNT_OFFSET]
    add    r2, #MIRROR_STRING_VALUE_OFFSET
    add    r1, #MIRROR_STRING_VALUE_OFFSET

    /*
     * At this point, we have:
     *    value:  r2/r1
     *    offset: r4/r9
     *    count:  r7/r10
     * We're going to compute
     *    r11 <- countDiff
     *    r10 <- minCount
     */
     subs  r11, r7, r10
     it    ls
     movls r10, r7

     /*
      * Note: data pointers point to previous element so we can use pre-index
      * mode with base writeback.
      */
     subs  r2, #2   @ offset to contents[-1]
     subs  r1, #2   @ offset to contents[-1]

     /*
      * At this point we have:
      *   r2: *this string data
      *   r1: *comp string data
      *   r10: iteration count for comparison
      *   r11: value to return if the first part of the string is equal
      *   r0: reserved for result
      *   r3, r4, r7, r8, r9, r12 available for loading string data
      */

    subs  r10, #2
    blt   .Ldo_remainder2

      /*
       * Unroll the first two checks so we can quickly catch early mismatch
       * on long strings (but preserve incoming alignment)
       */

    ldrh  r3, [r2, #2]!
    ldrh  r4, [r1, #2]!
    ldrh  r7, [r2, #2]!
    ldrh  r8, [r1, #2]!
    subs  r0, r3, r4
    it    eq
    subseq  r0, r7, r8
    bne   .Ldone
    cmp   r10, #28
    bgt   .Ldo_memcmp16
    subs  r10, #3
    blt   .Ldo_remainder

.Lloopback_triple:
    ldrh  r3, [r2, #2]!
    ldrh  r4, [r1, #2]!
    ldrh  r7, [r2, #2]!
    ldrh  r8, [r1, #2]!
    ldrh  r9, [r2, #2]!
    ldrh  r12,[r1, #2]!
    subs  r0, r3, r4
    it    eq
    subseq  r0, r7, r8
    it    eq
    subseq  r0, r9, r12
    bne   .Ldone
    subs  r10, #3
    bge   .Lloopback_triple

.Ldo_remainder:
    adds  r10, #3
    beq   .Lreturn_diff

.Lloopback_single:
    ldrh  r3, [r2, #2]!
    ldrh  r4, [r1, #2]!
    subs  r0, r3, r4
    bne   .Ldone
    subs  r10, #1
    bne   .Lloopback_single

.Lreturn_diff:
    mov   r0, r11
    pop   {r4, r7-r12, pc}

.Ldo_remainder2:
    adds  r10, #2
    bne   .Lloopback_single
    mov   r0, r11
    pop   {r4, r7-r12, pc}

    /* Long string case */
.Ldo_memcmp16:
    mov   r7, r11
    add   r0, r2, #2
    add   r1, r1, #2
    mov   r2, r10
    bl    __memcmp16
    cmp   r0, #0
    it    eq
    moveq r0, r7
.Ldone:
    pop   {r4, r7-r12, pc}
END art_quick_string_compareto

    /* Assembly routines used to handle ABI differences. */

    /* double fmod(double a, double b) */
    .extern fmod
ENTRY art_quick_fmod
    push  {lr}
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset lr, 0
    sub   sp, #4
    .cfi_adjust_cfa_offset 4
    vmov  r0, r1, d0
    vmov  r2, r3, d1
    bl    fmod
    vmov  d0, r0, r1
    add   sp, #4
    .cfi_adjust_cfa_offset -4
    pop   {pc}
END art_quick_fmod

    /* float fmodf(float a, float b) */
     .extern fmodf
ENTRY art_quick_fmodf
    push  {lr}
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset lr, 0
    sub   sp, #4
    .cfi_adjust_cfa_offset 4
    vmov  r0, r1, d0
    bl    fmodf
    vmov  s0, r0
    add   sp, #4
    .cfi_adjust_cfa_offset -4
    pop   {pc}
END art_quick_fmod

    /* int64_t art_d2l(double d) */
    .extern art_d2l
ENTRY art_quick_d2l
    vmov  r0, r1, d0
    b     art_d2l
END art_quick_d2l

    /* int64_t art_f2l(float f) */
    .extern art_f2l
ENTRY art_quick_f2l
    vmov  r0, s0
    b     art_f2l
END art_quick_f2l
