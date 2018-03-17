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

#include "arch/quick_alloc_entrypoints.S"

    .set noreorder
    .balign 4

    /* Deliver the given exception */
    .extern artDeliverExceptionFromCode
    /* Deliver an exception pending on a thread */
    .extern artDeliverPendingExceptionFromCode

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     * callee-save: $s0-$s8 + $gp + $ra, 11 total + 1 word padding + 4 open words for args
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, -64
    .cfi_adjust_cfa_offset 64

     // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVE != 64)
#error "SAVE_ALL_CALLEE_SAVE_FRAME(MIPS) size not as expected."
#endif

    sw     $ra, 60($sp)
    .cfi_rel_offset 31, 60
    sw     $s8, 56($sp)
    .cfi_rel_offset 30, 56
    sw     $gp, 52($sp)
    .cfi_rel_offset 28, 52
    sw     $s7, 48($sp)
    .cfi_rel_offset 23, 48
    sw     $s6, 44($sp)
    .cfi_rel_offset 22, 44
    sw     $s5, 40($sp)
    .cfi_rel_offset 21, 40
    sw     $s4, 36($sp)
    .cfi_rel_offset 20, 36
    sw     $s3, 32($sp)
    .cfi_rel_offset 19, 32
    sw     $s2, 28($sp)
    .cfi_rel_offset 18, 28
    sw     $s1, 24($sp)
    .cfi_rel_offset 17, 24
    sw     $s0, 20($sp)
    .cfi_rel_offset 16, 20
    # 1 word for alignment, 4 open words for args $a0-$a3, bottom will hold Method*
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly). Restoration assumes non-moving GC.
     * Does not include rSUSPEND or rSELF
     * callee-save: $s2-$s8 + $gp + $ra, 9 total + 3 words padding + 4 open words for args
     */
.macro SETUP_REF_ONLY_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, -64
    .cfi_adjust_cfa_offset 64

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE != 64)
#error "REFS_ONLY_CALLEE_SAVE_FRAME(MIPS) size not as expected."
#endif

    sw     $ra, 60($sp)
    .cfi_rel_offset 31, 60
    sw     $s8, 56($sp)
    .cfi_rel_offset 30, 56
    sw     $gp, 52($sp)
    .cfi_rel_offset 28, 52
    sw     $s7, 48($sp)
    .cfi_rel_offset 23, 48
    sw     $s6, 44($sp)
    .cfi_rel_offset 22, 44
    sw     $s5, 40($sp)
    .cfi_rel_offset 21, 40
    sw     $s4, 36($sp)
    .cfi_rel_offset 20, 36
    sw     $s3, 32($sp)
    .cfi_rel_offset 19, 32
    sw     $s2, 28($sp)
    .cfi_rel_offset 18, 28
    # 3 words for alignment and extra args, 4 open words for args $a0-$a3, bottom will hold Method*
.endm

.macro RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    lw     $ra, 60($sp)
    .cfi_restore 31
    lw     $s8, 56($sp)
    .cfi_restore 30
    lw     $gp, 52($sp)
    .cfi_restore 28
    lw     $s7, 48($sp)
    .cfi_restore 23
    lw     $s6, 44($sp)
    .cfi_restore 22
    lw     $s5, 40($sp)
    .cfi_restore 21
    lw     $s4, 36($sp)
    .cfi_restore 20
    lw     $s3, 32($sp)
    .cfi_restore 19
    lw     $s2, 28($sp)
    .cfi_restore 18
    addiu  $sp, $sp, 64
    .cfi_adjust_cfa_offset -64
.endm

.macro RESTORE_REF_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
    lw     $ra, 60($sp)
    .cfi_restore 31
    lw     $s8, 56($sp)
    .cfi_restore 30
    lw     $gp, 52($sp)
    .cfi_restore 28
    lw     $s7, 48($sp)
    .cfi_restore 23
    lw     $s6, 44($sp)
    .cfi_restore 22
    lw     $s5, 40($sp)
    .cfi_restore 21
    lw     $s4, 36($sp)
    .cfi_restore 20
    lw     $s3, 32($sp)
    .cfi_restore 19
    lw     $s2, 28($sp)
    .cfi_restore 18
    jr     $ra
    addiu  $sp, $sp, 64
    .cfi_adjust_cfa_offset -64
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs). Restoration assumes non-moving GC.
     * callee-save: $a1-$a3, $s2-$s8 + $gp + $ra, 12 total + 3 words padding + method*
     */
.macro SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, -64
    .cfi_adjust_cfa_offset 64

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE != 64)
#error "REFS_AND_ARGS_CALLEE_SAVE_FRAME(MIPS) size not as expected."
#endif

    sw     $ra, 60($sp)
    .cfi_rel_offset 31, 60
    sw     $s8, 56($sp)
    .cfi_rel_offset 30, 56
    sw     $gp, 52($sp)
    .cfi_rel_offset 28, 52
    sw     $s7, 48($sp)
    .cfi_rel_offset 23, 48
    sw     $s6, 44($sp)
    .cfi_rel_offset 22, 44
    sw     $s5, 40($sp)
    .cfi_rel_offset 21, 40
    sw     $s4, 36($sp)
    .cfi_rel_offset 20, 36
    sw     $s3, 32($sp)
    .cfi_rel_offset 19, 32
    sw     $s2, 28($sp)
    .cfi_rel_offset 18, 28
    sw     $a3, 12($sp)
    .cfi_rel_offset 7, 12
    sw     $a2, 8($sp)
    .cfi_rel_offset 6, 8
    sw     $a1, 4($sp)
    .cfi_rel_offset 5, 4
    # bottom will hold Method*
.endm

.macro RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    lw     $ra, 60($sp)
    .cfi_restore 31
    lw     $s8, 56($sp)
    .cfi_restore 30
    lw     $gp, 52($sp)
    .cfi_restore 28
    lw     $s7, 48($sp)
    .cfi_restore 23
    lw     $s6, 44($sp)
    .cfi_restore 22
    lw     $s5, 40($sp)
    .cfi_restore 21
    lw     $s4, 36($sp)
    .cfi_restore 20
    lw     $s3, 32($sp)
    .cfi_restore 19
    lw     $s2, 28($sp)
    .cfi_restore 18
    lw     $a3, 12($sp)
    .cfi_restore 7
    lw     $a2, 8($sp)
    .cfi_restore 6
    lw     $a1, 4($sp)
    .cfi_restore 5
    addiu  $sp, $sp, 64           # pop frame
    .cfi_adjust_cfa_offset -64
.endm

    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_
     */
.macro DELIVER_PENDING_EXCEPTION
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME     # save callee saves for throw
    move    $a0, rSELF                   # pass Thread::Current
    la      $t9, artDeliverPendingExceptionFromCode
    jr      $t9                          # artDeliverPendingExceptionFromCode(Thread*, $sp)
    move    $a1, $sp                     # pass $sp
.endm

.macro RETURN_IF_NO_EXCEPTION
    lw     $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    bnez   $t0, 1f                       # success if no exception is pending
    nop
    jr     $ra
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_ZERO
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    bnez   $v0, 1f                       # success?
    nop
    jr     $ra                           # return on success
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    beqz   $v0, 1f                       # success?
    nop
    jr     $ra                           # return on success
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

    /*
     * On entry $a0 is uint32_t* gprs_ and $a1 is uint32_t* fprs_
     * FIXME: just guessing about the shape of the jmpbuf.  Where will pc be?
     */
ENTRY art_quick_do_long_jump
    l.s     $f0, 0($a1)
    l.s     $f1, 4($a1)
    l.s     $f2, 8($a1)
    l.s     $f3, 12($a1)
    l.s     $f4, 16($a1)
    l.s     $f5, 20($a1)
    l.s     $f6, 24($a1)
    l.s     $f7, 28($a1)
    l.s     $f8, 32($a1)
    l.s     $f9, 36($a1)
    l.s     $f10, 40($a1)
    l.s     $f11, 44($a1)
    l.s     $f12, 48($a1)
    l.s     $f13, 52($a1)
    l.s     $f14, 56($a1)
    l.s     $f15, 60($a1)
    l.s     $f16, 64($a1)
    l.s     $f17, 68($a1)
    l.s     $f18, 72($a1)
    l.s     $f19, 76($a1)
    l.s     $f20, 80($a1)
    l.s     $f21, 84($a1)
    l.s     $f22, 88($a1)
    l.s     $f23, 92($a1)
    l.s     $f24, 96($a1)
    l.s     $f25, 100($a1)
    l.s     $f26, 104($a1)
    l.s     $f27, 108($a1)
    l.s     $f28, 112($a1)
    l.s     $f29, 116($a1)
    l.s     $f30, 120($a1)
    l.s     $f31, 124($a1)
    .set push
    .set nomacro
    .set noat
    lw      $at, 4($a0)
    .set pop
    lw      $v0, 8($a0)
    lw      $v1, 12($a0)
    lw      $a1, 20($a0)
    lw      $a2, 24($a0)
    lw      $a3, 28($a0)
    lw      $t0, 32($a0)
    lw      $t1, 36($a0)
    lw      $t2, 40($a0)
    lw      $t3, 44($a0)
    lw      $t4, 48($a0)
    lw      $t5, 52($a0)
    lw      $t6, 56($a0)
    lw      $t7, 60($a0)
    lw      $s0, 64($a0)
    lw      $s1, 68($a0)
    lw      $s2, 72($a0)
    lw      $s3, 76($a0)
    lw      $s4, 80($a0)
    lw      $s5, 84($a0)
    lw      $s6, 88($a0)
    lw      $s7, 92($a0)
    lw      $t8, 96($a0)
    lw      $t9, 100($a0)
    lw      $gp, 112($a0)
    lw      $sp, 116($a0)
    lw      $fp, 120($a0)
    lw      $ra, 124($a0)
    lw      $a0, 16($a0)
    move    $v0, $zero          # clear result registers r0 and r1
    jr      $ra                 # do long jump
    move    $v1, $zero
END art_quick_do_long_jump

    /*
     * Called by managed code, saves most registers (forms basis of long jump context) and passes
     * the bottom of the stack. artDeliverExceptionFromCode will place the callee save Method* at
     * the bottom of the thread. On entry r0 holds Throwable*
     */
ENTRY art_quick_deliver_exception
    GENERATE_GLOBAL_POINTER
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a1, rSELF                 # pass Thread::Current
    la   $t9, artDeliverExceptionFromCode
    jr   $t9                        # artDeliverExceptionFromCode(Throwable*, Thread*, $sp)
    move $a2, $sp                   # pass $sp
END art_quick_deliver_exception

    /*
     * Called by managed code to create and deliver a NullPointerException
     */
    .extern artThrowNullPointerExceptionFromCode
ENTRY art_quick_throw_null_pointer_exception
    GENERATE_GLOBAL_POINTER
.Lart_quick_throw_null_pointer_exception_gp_set:
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a0, rSELF                 # pass Thread::Current
    la   $t9, artThrowNullPointerExceptionFromCode
    jr   $t9                        # artThrowNullPointerExceptionFromCode(Thread*, $sp)
    move $a1, $sp                   # pass $sp
END art_quick_throw_null_pointer_exception

    /*
     * Called by managed code to create and deliver an ArithmeticException
     */
    .extern artThrowDivZeroFromCode
ENTRY art_quick_throw_div_zero
    GENERATE_GLOBAL_POINTER
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a0, rSELF                 # pass Thread::Current
    la   $t9, artThrowDivZeroFromCode
    jr   $t9                        # artThrowDivZeroFromCode(Thread*, $sp)
    move $a1, $sp                   # pass $sp
END art_quick_throw_div_zero

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException
     */
    .extern artThrowArrayBoundsFromCode
ENTRY art_quick_throw_array_bounds
    GENERATE_GLOBAL_POINTER
.Lart_quick_throw_array_bounds_gp_set:
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a2, rSELF                 # pass Thread::Current
    la   $t9, artThrowArrayBoundsFromCode
    jr   $t9                        # artThrowArrayBoundsFromCode(index, limit, Thread*, $sp)
    move $a3, $sp                   # pass $sp
END art_quick_throw_array_bounds

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
    .extern artThrowStackOverflowFromCode
ENTRY art_quick_throw_stack_overflow
    GENERATE_GLOBAL_POINTER
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a0, rSELF                 # pass Thread::Current
    la   $t9, artThrowStackOverflowFromCode
    jr   $t9                        # artThrowStackOverflowFromCode(Thread*, $sp)
    move $a1, $sp                   # pass $sp
END art_quick_throw_stack_overflow

    /*
     * Called by managed code to create and deliver a NoSuchMethodError.
     */
    .extern artThrowNoSuchMethodFromCode
ENTRY art_quick_throw_no_such_method
    GENERATE_GLOBAL_POINTER
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a1, rSELF                 # pass Thread::Current
    la   $t9, artThrowNoSuchMethodFromCode
    jr   $t9                        # artThrowNoSuchMethodFromCode(method_idx, Thread*, $sp)
    move $a2, $sp                   # pass $sp
END art_quick_throw_no_such_method

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/$a0 with the target Method*, arg0/$a0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, load the caller's Method*, align the
     * stack and call the appropriate C helper.
     * NOTE: "this" is first visable argument of the target, and so can be found in arg1/$a1.
     *
     * The helper will attempt to locate the target and return a 64-bit result in $v0/$v1 consisting
     * of the target Method* in $v0 and method->code_ in $v1.
     *
     * If unsuccessful, the helper will return NULL/NULL. There will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
.macro INVOKE_TRAMPOLINE c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    GENERATE_GLOBAL_POINTER
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME  # save callee saves in case allocation triggers GC
    lw    $a2, 64($sp)                    # pass caller Method*
    move  $t0, $sp                        # save $sp
    addiu $sp, $sp, -32                   # make space for extra args
    .cfi_adjust_cfa_offset 32
    move  $a3, rSELF                      # pass Thread::Current
    .cfi_rel_offset 28, 12
    jal   \cxx_name                       # (method_idx, this, caller, Thread*, $sp)
    sw    $t0, 16($sp)                    # pass $sp
    addiu $sp, $sp, 32                    # release out args
    .cfi_adjust_cfa_offset -32
    move  $a0, $v0                        # save target Method*
    move  $t9, $v1                        # save $v0->code_
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    beqz  $v0, 1f
    nop
    jr    $t9
    nop
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
     * Invocation stub for quick code.
     * On entry:
     *   a0 = method pointer
     *   a1 = argument array or NULL for no argument methods
     *   a2 = size of argument array in bytes
     *   a3 = (managed) thread pointer
     *   [sp + 16] = JValue* result
     *   [sp + 20] = shorty
     */
ENTRY art_quick_invoke_stub
    GENERATE_GLOBAL_POINTER
    sw    $a0, 0($sp)           # save out a0
    addiu $sp, $sp, -16         # spill s0, s1, fp, ra
    .cfi_adjust_cfa_offset 16
    sw    $ra, 12($sp)
    .cfi_rel_offset 31, 12
    sw    $fp, 8($sp)
    .cfi_rel_offset 30, 8
    sw    $s1, 4($sp)
    .cfi_rel_offset 17, 4
    sw    $s0, 0($sp)
    .cfi_rel_offset 16, 0
    move  $fp, $sp              # save sp in fp
    .cfi_def_cfa_register 30
    move  $s1, $a3              # move managed thread pointer into s1
    addiu $s0, $zero, SUSPEND_CHECK_INTERVAL  # reset s0 to suspend check interval
    addiu $t0, $a2, 16          # create space for method pointer in frame
    srl   $t0, $t0, 4           # shift the frame size right 4
    sll   $t0, $t0, 4           # shift the frame size left 4 to align to 16 bytes
    subu  $sp, $sp, $t0         # reserve stack space for argument array
    addiu $a0, $sp, 4           # pass stack pointer + method ptr as dest for memcpy
    jal   memcpy                # (dest, src, bytes)
    addiu $sp, $sp, -16         # make space for argument slots for memcpy
    addiu $sp, $sp, 16          # restore stack after memcpy
    lw    $a0, 16($fp)          # restore method*
    lw    $a1, 4($sp)           # copy arg value for a1
    lw    $a2, 8($sp)           # copy arg value for a2
    lw    $a3, 12($sp)          # copy arg value for a3
    lw    $t9, METHOD_QUICK_CODE_OFFSET_32($a0)  # get pointer to the code
    jalr  $t9                   # call the method
    sw    $zero, 0($sp)         # store NULL for method* at bottom of frame
    move  $sp, $fp              # restore the stack
    lw    $s0, 0($sp)
    .cfi_restore 16
    lw    $s1, 4($sp)
    .cfi_restore 17
    lw    $fp, 8($sp)
    .cfi_restore 30
    lw    $ra, 12($sp)
    .cfi_restore 31
    addiu $sp, $sp, 16
    .cfi_adjust_cfa_offset -16
    lw    $t0, 16($sp)          # get result pointer
    lw    $t1, 20($sp)          # get shorty
    lb    $t1, 0($t1)           # get result type char
    li    $t2, 68               # put char 'D' into t2
    beq   $t1, $t2, 1f          # branch if result type char == 'D'
    li    $t3, 70               # put char 'F' into t3
    beq   $t1, $t3, 1f          # branch if result type char == 'F'
    sw    $v0, 0($t0)           # store the result
    jr    $ra
    sw    $v1, 4($t0)           # store the other half of the result
1:
    s.s   $f0, 0($t0)           # store floating point result
    jr    $ra
    s.s   $f1, 4($t0)           # store other half of floating point result
END art_quick_invoke_stub

    /*
     * Entry from managed code that calls artHandleFillArrayDataFromCode and delivers exception on
     * failure.
     */
    .extern artHandleFillArrayDataFromCode
ENTRY art_quick_handle_fill_data
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  # save callee saves in case exception allocation triggers GC
    move    $a2, rSELF                         # pass Thread::Current
    jal     artHandleFillArrayDataFromCode     # (Array*, const DexFile::Payload*, Thread*, $sp)
    move    $a3, $sp                           # pass $sp
    RETURN_IF_ZERO
END art_quick_handle_fill_data

    /*
     * Entry from managed code that calls artLockObjectFromCode, may block for GC.
     */
    .extern artLockObjectFromCode
ENTRY art_quick_lock_object
    GENERATE_GLOBAL_POINTER
    beqz    $a0, .Lart_quick_throw_null_pointer_exception_gp_set
    nop
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME      # save callee saves in case we block
    move    $a1, rSELF                    # pass Thread::Current
    jal     artLockObjectFromCode         # (Object* obj, Thread*, $sp)
    move    $a2, $sp                      # pass $sp
    RETURN_IF_ZERO
END art_quick_lock_object

    /*
     * Entry from managed code that calls artUnlockObjectFromCode and delivers exception on failure.
     */
    .extern artUnlockObjectFromCode
ENTRY art_quick_unlock_object
    GENERATE_GLOBAL_POINTER
    beqz    $a0, .Lart_quick_throw_null_pointer_exception_gp_set
    nop
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  # save callee saves in case exception allocation triggers GC
    move    $a1, rSELF                # pass Thread::Current
    jal     artUnlockObjectFromCode   # (Object* obj, Thread*, $sp)
    move    $a2, $sp                  # pass $sp
    RETURN_IF_ZERO
END art_quick_unlock_object

    /*
     * Entry from managed code that calls artCheckCastFromCode and delivers exception on failure.
     */
    .extern artThrowClassCastException
ENTRY art_quick_check_cast
    GENERATE_GLOBAL_POINTER
    addiu  $sp, $sp, -16
    .cfi_adjust_cfa_offset 16
    sw     $ra, 12($sp)
    .cfi_rel_offset 31, 12
    sw     $t9, 8($sp)
    sw     $a1, 4($sp)
    sw     $a0, 0($sp)
    jal    artIsAssignableFromCode
    nop
    beqz   $v0, .Lthrow_class_cast_exception
    lw     $ra, 12($sp)
    jr     $ra
    addiu  $sp, $sp, 16
    .cfi_adjust_cfa_offset -16
.Lthrow_class_cast_exception:
    lw     $t9, 8($sp)
    lw     $a1, 4($sp)
    lw     $a0, 0($sp)
    addiu  $sp, $sp, 16
    .cfi_adjust_cfa_offset -16
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a2, rSELF                 # pass Thread::Current
    la   $t9, artThrowClassCastException
    jr   $t9                        # artThrowClassCastException (Class*, Class*, Thread*, SP)
    move $a3, $sp                   # pass $sp
END art_quick_check_cast

    /*
     * Entry from managed code for array put operations of objects where the value being stored
     * needs to be checked for compatibility.
     * a0 = array, a1 = index, a2 = value
     */
ENTRY art_quick_aput_obj_with_null_and_bound_check
    GENERATE_GLOBAL_POINTER
    bnez    $a0, .Lart_quick_aput_obj_with_bound_check_gp_set
    nop
    b .Lart_quick_throw_null_pointer_exception_gp_set
    nop
END art_quick_aput_obj_with_null_and_bound_check

ENTRY art_quick_aput_obj_with_bound_check
    GENERATE_GLOBAL_POINTER
.Lart_quick_aput_obj_with_bound_check_gp_set:
    lw $t0, ARRAY_LENGTH_OFFSET($a0)
    sltu $t1, $a1, $t0
    bnez $t1, .Lart_quick_aput_obj_gp_set
    nop
    move $a0, $a1
    b .Lart_quick_throw_array_bounds_gp_set
    move $a1, $t0
END art_quick_aput_obj_with_bound_check

ENTRY art_quick_aput_obj
    GENERATE_GLOBAL_POINTER
.Lart_quick_aput_obj_gp_set:
    beqz $a2, .Ldo_aput_null
    nop
    lw $t0, CLASS_OFFSET($a0)
    lw $t1, CLASS_OFFSET($a2)
    lw $t0, CLASS_COMPONENT_TYPE_OFFSET($t0)
    bne $t1, $t0, .Lcheck_assignability  # value's type == array's component type - trivial assignability
    nop
.Ldo_aput:
    sll $a1, $a1, 2
    add $t0, $a0, $a1
    sw  $a2, OBJECT_ARRAY_DATA_OFFSET($t0)
    lw  $t0, THREAD_CARD_TABLE_OFFSET(rSELF)
    srl $t1, $a0, 7
    add $t1, $t1, $t0
    sb  $t0, ($t1)
    jr  $ra
    nop
.Ldo_aput_null:
    sll $a1, $a1, 2
    add $t0, $a0, $a1
    sw  $a2, OBJECT_ARRAY_DATA_OFFSET($t0)
    jr  $ra
    nop
.Lcheck_assignability:
    addiu  $sp, $sp, -32
    .cfi_adjust_cfa_offset 32
    sw     $ra, 28($sp)
    .cfi_rel_offset 31, 28
    sw     $t9, 12($sp)
    sw     $a2, 8($sp)
    sw     $a1, 4($sp)
    sw     $a0, 0($sp)
    move   $a1, $t1
    move   $a0, $t0
    jal    artIsAssignableFromCode  # (Class*, Class*)
    nop
    lw     $ra, 28($sp)
    lw     $t9, 12($sp)
    lw     $a2, 8($sp)
    lw     $a1, 4($sp)
    lw     $a0, 0($sp)
    add    $sp, 32
    .cfi_adjust_cfa_offset -32
    bnez   $v0, .Ldo_aput
    nop
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a1, $a2
    move $a2, rSELF                 # pass Thread::Current
    la   $t9, artThrowArrayStoreException
    jr   $t9                        # artThrowArrayStoreException(Class*, Class*, Thread*, SP)
    move $a3, $sp                   # pass $sp
END art_quick_aput_obj

    /*
     * Entry from managed code when uninitialized static storage, this stub will run the class
     * initializer and deliver the exception on error. On success the static storage base is
     * returned.
     */
    .extern artInitializeStaticStorageFromCode
ENTRY art_quick_initialize_static_storage
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME            # save callee saves in case of GC
    move    $a2, rSELF                          # pass Thread::Current
    # artInitializeStaticStorageFromCode(uint32_t type_idx, Method* referrer, Thread*, $sp)
    jal     artInitializeStaticStorageFromCode
    move    $a3, $sp                            # pass $sp
    RETURN_IF_RESULT_IS_NON_ZERO
END art_quick_initialize_static_storage

    /*
     * Entry from managed code when dex cache misses for a type_idx.
     */
    .extern artInitializeTypeFromCode
ENTRY art_quick_initialize_type
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME           # save callee saves in case of GC
    move    $a2, rSELF                         # pass Thread::Current
    # artInitializeTypeFromCode(uint32_t type_idx, Method* referrer, Thread*, $sp)
    jal     artInitializeTypeFromCode
    move    $a3, $sp                           # pass $sp
    RETURN_IF_RESULT_IS_NON_ZERO
END art_quick_initialize_type

    /*
     * Entry from managed code when type_idx needs to be checked for access and dex cache may also
     * miss.
     */
    .extern artInitializeTypeAndVerifyAccessFromCode
ENTRY art_quick_initialize_type_and_verify_access
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME           # save callee saves in case of GC
    move    $a2, rSELF                         # pass Thread::Current
    # artInitializeTypeFromCode(uint32_t type_idx, Method* referrer, Thread*, $sp)
    jal     artInitializeTypeAndVerifyAccessFromCode
    move    $a3, $sp                           # pass $sp
    RETURN_IF_RESULT_IS_NON_ZERO
END art_quick_initialize_type_and_verify_access

    /*
     * Called by managed code to resolve a static field and load a 32-bit primitive value.
     */
    .extern artGet32StaticFromCode
ENTRY art_quick_get32_static
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a1, 64($sp)                  # pass referrer's Method*
    move   $a2, rSELF                    # pass Thread::Current
    jal    artGet32StaticFromCode        # (uint32_t field_idx, const Method* referrer, Thread*, $sp)
    move   $a3, $sp                      # pass $sp
    RETURN_IF_NO_EXCEPTION
END art_quick_get32_static

    /*
     * Called by managed code to resolve a static field and load a 64-bit primitive value.
     */
    .extern artGet64StaticFromCode
ENTRY art_quick_get64_static
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a1, 64($sp)                  # pass referrer's Method*
    move   $a2, rSELF                    # pass Thread::Current
    jal    artGet64StaticFromCode        # (uint32_t field_idx, const Method* referrer, Thread*, $sp)
    move   $a3, $sp                      # pass $sp
    RETURN_IF_NO_EXCEPTION
END art_quick_get64_static

    /*
     * Called by managed code to resolve a static field and load an object reference.
     */
    .extern artGetObjStaticFromCode
ENTRY art_quick_get_obj_static
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a1, 64($sp)                  # pass referrer's Method*
    move   $a2, rSELF                    # pass Thread::Current
    jal    artGetObjStaticFromCode       # (uint32_t field_idx, const Method* referrer, Thread*, $sp)
    move   $a3, $sp                      # pass $sp
    RETURN_IF_NO_EXCEPTION
END art_quick_get_obj_static

    /*
     * Called by managed code to resolve an instance field and load a 32-bit primitive value.
     */
    .extern artGet32InstanceFromCode
ENTRY art_quick_get32_instance
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a2, 64($sp)                  # pass referrer's Method*
    move   $a3, rSELF                    # pass Thread::Current
    jal    artGet32InstanceFromCode      # (field_idx, Object*, referrer, Thread*, $sp)
    sw     $sp, 16($sp)                  # pass $sp
    RETURN_IF_NO_EXCEPTION
END art_quick_get32_instance

    /*
     * Called by managed code to resolve an instance field and load a 64-bit primitive value.
     */
    .extern artGet64InstanceFromCode
ENTRY art_quick_get64_instance
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a2, 64($sp)                  # pass referrer's Method*
    move   $a3, rSELF                    # pass Thread::Current
    jal    artGet64InstanceFromCode      # (field_idx, Object*, referrer, Thread*, $sp)
    sw     $sp, 16($sp)                  # pass $sp
    RETURN_IF_NO_EXCEPTION
END art_quick_get64_instance

    /*
     * Called by managed code to resolve an instance field and load an object reference.
     */
    .extern artGetObjInstanceFromCode
ENTRY art_quick_get_obj_instance
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a2, 64($sp)                  # pass referrer's Method*
    move   $a3, rSELF                    # pass Thread::Current
    jal    artGetObjInstanceFromCode     # (field_idx, Object*, referrer, Thread*, $sp)
    sw     $sp, 16($sp)                  # pass $sp
    RETURN_IF_NO_EXCEPTION
END art_quick_get_obj_instance

    /*
     * Called by managed code to resolve a static field and store a 32-bit primitive value.
     */
    .extern artSet32StaticFromCode
ENTRY art_quick_set32_static
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a2, 64($sp)                  # pass referrer's Method*
    move   $a3, rSELF                    # pass Thread::Current
    jal    artSet32StaticFromCode        # (field_idx, new_val, referrer, Thread*, $sp)
    sw     $sp, 16($sp)                  # pass $sp
    RETURN_IF_ZERO
END art_quick_set32_static

    /*
     * Called by managed code to resolve a static field and store a 64-bit primitive value.
     */
    .extern artSet32StaticFromCode
ENTRY art_quick_set64_static
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a1, 64($sp)                  # pass referrer's Method*
    sw     rSELF, 16($sp)                # pass Thread::Current
    jal    artSet64StaticFromCode        # (field_idx, referrer, new_val, Thread*, $sp)
    sw     $sp, 20($sp)                  # pass $sp
    RETURN_IF_ZERO
END art_quick_set64_static

    /*
     * Called by managed code to resolve a static field and store an object reference.
     */
    .extern artSetObjStaticFromCode
ENTRY art_quick_set_obj_static
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a2, 64($sp)                  # pass referrer's Method*
    move   $a3, rSELF                    # pass Thread::Current
    jal    artSetObjStaticFromCode       # (field_idx, new_val, referrer, Thread*, $sp)
    sw     $sp, 16($sp)                  # pass $sp
    RETURN_IF_ZERO
END art_quick_set_obj_static

    /*
     * Called by managed code to resolve an instance field and store a 32-bit primitive value.
     */
    .extern artSet32InstanceFromCode
ENTRY art_quick_set32_instance
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a3, 64($sp)                  # pass referrer's Method*
    sw     rSELF, 16($sp)                # pass Thread::Current
    jal    artSet32InstanceFromCode      # (field_idx, Object*, new_val, referrer, Thread*, $sp)
    sw     $sp, 20($sp)                  # pass $sp
    RETURN_IF_ZERO
END art_quick_set32_instance

    /*
     * Called by managed code to resolve an instance field and store a 64-bit primitive value.
     */
    .extern artSet32InstanceFromCode
ENTRY art_quick_set64_instance
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    sw     rSELF, 16($sp)                # pass Thread::Current
    jal    artSet64InstanceFromCode      # (field_idx, Object*, new_val, Thread*, $sp)
    sw     $sp, 20($sp)                  # pass $sp
    RETURN_IF_ZERO
END art_quick_set64_instance

    /*
     * Called by managed code to resolve an instance field and store an object reference.
     */
    .extern artSetObjInstanceFromCode
ENTRY art_quick_set_obj_instance
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case of GC
    lw     $a3, 64($sp)                  # pass referrer's Method*
    sw     rSELF, 16($sp)                # pass Thread::Current
    jal    artSetObjInstanceFromCode     # (field_idx, Object*, new_val, referrer, Thread*, $sp)
    sw     $sp, 20($sp)                  # pass $sp
    RETURN_IF_ZERO
END art_quick_set_obj_instance

    /*
     * Entry from managed code to resolve a string, this stub will allocate a String and deliver an
     * exception on error. On success the String is returned. R0 holds the referring method,
     * R1 holds the string index. The fast path check for hit in strings cache has already been
     * performed.
     */
    .extern artResolveStringFromCode
ENTRY art_quick_resolve_string
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  # save callee saves in case of GC
    move    $a2, rSELF                # pass Thread::Current
    # artResolveStringFromCode(Method* referrer, uint32_t string_idx, Thread*, $sp)
    jal     artResolveStringFromCode
    move    $a3, $sp                  # pass $sp
    RETURN_IF_RESULT_IS_NON_ZERO
END art_quick_resolve_string


// Macro to facilitate adding new allocation entrypoints.
.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  # save callee saves in case of GC
    move    $a2, rSELF                # pass Thread::Current
    jal     \entrypoint
    move    $a3, $sp                  # pass $sp
    \return
END \name
.endm

.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    GENERATE_GLOBAL_POINTER
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  # save callee saves in case of GC
    move    $a3, rSELF                # pass Thread::Current
    jal     \entrypoint
    sw      $sp, 16($sp)              # pass $sp
    \return
END \name
.endm

// Generate the allocation entrypoints for each allocator.
GENERATE_ALL_ALLOC_ENTRYPOINTS

    /*
     * Called by managed code when the value in rSUSPEND has been decremented to 0.
     */
    .extern artTestSuspendFromCode
ENTRY art_quick_test_suspend
    GENERATE_GLOBAL_POINTER
    lh     $a0, THREAD_FLAGS_OFFSET(rSELF)
    bnez   $a0, 1f
    addi  rSUSPEND, $zero, SUSPEND_CHECK_INTERVAL   # reset rSUSPEND to SUSPEND_CHECK_INTERVAL
    jr     $ra
    nop
1:
    move   $a0, rSELF
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME          # save callee saves for stack crawl
    jal    artTestSuspendFromCode             # (Thread*, $sp)
    move   $a1, $sp
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
END art_quick_test_suspend

    /*
     * Called by managed code that is attempting to call a method on a proxy class. On entry
     * r0 holds the proxy method; r1, r2 and r3 may contain arguments.
     */
    .extern artQuickProxyInvokeHandler
ENTRY art_quick_proxy_invoke_handler
    GENERATE_GLOBAL_POINTER
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    sw      $a0, 0($sp)            # place proxy method at bottom of frame
    move    $a2, rSELF             # pass Thread::Current
    jal     artQuickProxyInvokeHandler  # (Method* proxy method, receiver, Thread*, SP)
    move    $a3, $sp               # pass $sp
    lw      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    bnez    $t0, 1f
    mtc1    $v0, $f0               # place return value to FP return value
    jr      $ra
    mtc1    $v1, $f1               # place return value to FP return value
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict. t0 is a hidden argument that holds the target method's
     * dex method index.
     */
ENTRY art_quick_imt_conflict_trampoline
    GENERATE_GLOBAL_POINTER
    lw      $a0, 0($sp)            # load caller Method*
    lw      $a0, METHOD_DEX_CACHE_METHODS_OFFSET($a0)  # load dex_cache_resolved_methods
    sll     $t0, 2                 # convert target method offset to bytes
    add     $a0, $t0               # get address of target method
    lw      $a0, OBJECT_ARRAY_DATA_OFFSET($a0)  # load the target method
    la      $t9, art_quick_invoke_interface_trampoline
    jr      $t9
END art_quick_imt_conflict_trampoline

    .extern artQuickResolutionTrampoline
ENTRY art_quick_resolution_trampoline
    GENERATE_GLOBAL_POINTER
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    move    $a2, rSELF             # pass Thread::Current
    jal     artQuickResolutionTrampoline  # (Method* called, receiver, Thread*, SP)
    move    $a3, $sp               # pass $sp
    beqz    $v0, 1f
    lw      $a0, 0($sp)            # load resolved method to $a0
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    move    $t9, $v0               # code pointer must be in $t9 to generate the global pointer
    jr      $v0                    # tail call to method
    nop
1:
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_resolution_trampoline

UNIMPLEMENTED art_quick_generic_jni_trampoline

    .extern artQuickToInterpreterBridge
ENTRY art_quick_to_interpreter_bridge
    GENERATE_GLOBAL_POINTER
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    move    $a1, rSELF             # pass Thread::Current
    jal     artQuickToInterpreterBridge    # (Method* method, Thread*, SP)
    move    $a2, $sp               # pass $sp
    lw      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    bnez    $t0, 1f
    mtc1    $v0, $f0               # place return value to FP return value
    jr      $ra
    mtc1    $v1, $f1               # place return value to FP return value
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
    .extern artInstrumentationMethodEntryFromCode
    .extern artInstrumentationMethodExitFromCode
ENTRY art_quick_instrumentation_entry
    GENERATE_GLOBAL_POINTER
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    move     $t0, $sp       # remember bottom of caller's frame
    addiu    $sp, $sp, -32  # space for args, pad (3 words), arguments (5 words)
    .cfi_adjust_cfa_offset 32
    sw       $a0, 28($sp)   # save arg0
    sw       $ra, 16($sp)   # pass $ra
    move     $a3, $t0       # pass $sp
    jal      artInstrumentationMethodEntryFromCode  # (Method*, Object*, Thread*, SP, LR)
    move     $a2, rSELF     # pass Thread::Current
    move     $t9, $v0       # $t9 holds reference to code
    lw       $a0, 28($sp)   # restore arg0
    addiu    $sp, $sp, 32   # remove args
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    jalr     $t9            # call method
    nop
END art_quick_instrumentation_entry
    /* intentional fallthrough */
    .global art_quick_instrumentation_exit
art_quick_instrumentation_exit:
    .cfi_startproc
    addiu    $t9, $ra, 4    # put current address into $t9 to rebuild $gp
    GENERATE_GLOBAL_POINTER
    move     $ra, $zero     # link register is to here, so clobber with 0 for later checks
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME
    move     $t0, $sp       # remember bottom of caller's frame
    addiu    $sp, $sp, -48  # save return values and set up args
    .cfi_adjust_cfa_offset 48
    sw       $v0, 32($sp)
    .cfi_rel_offset 2, 32
    sw       $v1, 36($sp)
    .cfi_rel_offset 3, 36 
    s.s      $f0, 40($sp)
    s.s      $f1, 44($sp)
    s.s      $f0, 16($sp)   # pass fpr result
    s.s      $f1, 20($sp)
    move     $a2, $v0       # pass gpr result
    move     $a3, $v1
    move     $a1, $t0       # pass $sp
    jal      artInstrumentationMethodExitFromCode  # (Thread*, SP, gpr_res, fpr_res)
    move     $a0, rSELF     # pass Thread::Current
    move     $t0, $v0       # set aside returned link register
    move     $ra, $v1       # set link register for deoptimization
    lw       $v0, 32($sp)   # restore return values
    lw       $v1, 36($sp)
    l.s      $f0, 40($sp)
    l.s      $f1, 44($sp)
    jr       $t0            # return
    addiu    $sp, $sp, 112  # 48 bytes of args + 64 bytes of callee save frame
    .cfi_adjust_cfa_offset -112
END art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimize
    .extern artEnterInterpreterFromDeoptimize
ENTRY art_quick_deoptimize
    GENERATE_GLOBAL_POINTER
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move     $a0, rSELF     # pass Thread::current
    jal      artDeoptimize  # artDeoptimize(Thread*, SP)
                            # Returns caller method's frame size.
    move     $a1, $sp       # pass $sp
END art_quick_deoptimize

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   $a0: low word
     *   $a1: high word
     *   $a2: shift count
     */
ENTRY art_quick_shl_long
    /* shl-long vAA, vBB, vCC */
    sll     $v0, $a0, $a2                    #  rlo<- alo << (shift&31)
    not     $v1, $a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     $a0, 1
    srl     $a0, $v1                         #  alo<- alo >> (32-(shift&31))
    sll     $v1, $a1, $a2                    #  rhi<- ahi << (shift&31)
    or      $v1, $a0                         #  rhi<- rhi | alo
    andi    $a2, 0x20                        #  shift< shift & 0x20
    movn    $v1, $v0, $a2                    #  rhi<- rlo (if shift&0x20)
    jr      $ra
    movn    $v0, $zero, $a2                  #  rlo<- 0  (if shift&0x20)
END art_quick_shl_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   $a0: low word
     *   $a1: high word
     *   $a2: shift count
     */
    .global art_quick_shr_long
ENTRY art_quick_shr_long
    sra     $v1, $a1, $a2                    #  rhi<- ahi >> (shift&31)
    srl     $v0, $a0, $a2                    #  rlo<- alo >> (shift&31)
    sra     $a3, $a1, 31                     #  $a3<- sign(ah)
    not     $a0, $a2                         #  alo<- 31-shift (shift is 5b)
    sll     $a1, 1
    sll     $a1, $a0                         #  ahi<- ahi << (32-(shift&31))
    or      $v0, $a1                         #  rlo<- rlo | ahi
    andi    $a2, 0x20                        #  shift & 0x20
    movn    $v0, $v1, $a2                    #  rlo<- rhi (if shift&0x20)
    jr      $ra
    movn    $v1, $a3, $a2                    #  rhi<- sign(ahi) (if shift&0x20)
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
    .global art_quick_ushr_long
ENTRY art_quick_ushr_long
    srl     $v1, $a1, $a2                    #  rhi<- ahi >> (shift&31)
    srl     $v0, $a0, $a2                    #  rlo<- alo >> (shift&31)
    not     $a0, $a2                         #  alo<- 31-shift (shift is 5b)
    sll     $a1, 1
    sll     $a1, $a0                         #  ahi<- ahi << (32-(shift&31))
    or      $v0, $a1                         #  rlo<- rlo | ahi
    andi    $a2, 0x20                        #  shift & 0x20
    movn    $v0, $v1, $a2                    #  rlo<- rhi (if shift&0x20)
    jr      $ra
    movn    $v1, $zero, $a2                  #  rhi<- 0 (if shift&0x20)
END art_quick_ushr_long

ENTRY art_quick_indexof
    jr $ra
    nop
END art_quick_indexof

ENTRY art_quick_string_compareto
    jr $ra
    nop
END art_quick_string_compareto
