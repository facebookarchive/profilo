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

#define ARG_SLOT_SIZE   32    // space for a0-a3 plus 4 more words

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     * Callee-save: $s0-$s8 + $gp + $ra, 11 total + 1 word for Method*
     * Clobbers $t0 and $sp
     * Allocates ARG_SLOT_SIZE bytes at the bottom of the stack for arg slots.
     * Reserves FRAME_SIZE_SAVE_ALL_CALLEE_SAVE + ARG_SLOT_SIZE bytes on the stack
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, -96
    .cfi_adjust_cfa_offset 96

     // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVE != 96)
#error "SAVE_ALL_CALLEE_SAVE_FRAME(MIPS) size not as expected."
#endif

    sw     $ra, 92($sp)
    .cfi_rel_offset 31, 92
    sw     $s8, 88($sp)
    .cfi_rel_offset 30, 88
    sw     $gp, 84($sp)
    .cfi_rel_offset 28, 84
    sw     $s7, 80($sp)
    .cfi_rel_offset 23, 80
    sw     $s6, 76($sp)
    .cfi_rel_offset 22, 76
    sw     $s5, 72($sp)
    .cfi_rel_offset 21, 72
    sw     $s4, 68($sp)
    .cfi_rel_offset 20, 68
    sw     $s3, 64($sp)
    .cfi_rel_offset 19, 64
    sw     $s2, 60($sp)
    .cfi_rel_offset 18, 60
    sw     $s1, 56($sp)
    .cfi_rel_offset 17, 56
    sw     $s0, 52($sp)
    .cfi_rel_offset 16, 52

    SDu $f30, $f31, 44, $sp, $t1
    SDu $f28, $f29, 36, $sp, $t1
    SDu $f26, $f27, 28, $sp, $t1
    SDu $f24, $f25, 20, $sp, $t1
    SDu $f22, $f23, 12, $sp, $t1
    SDu $f20, $f21, 4,  $sp, $t1

    # 1 word for holding Method*

    lw $t0, %got(_ZN3art7Runtime9instance_E)($gp)
    lw $t0, 0($t0)
    THIS_LOAD_REQUIRES_READ_BARRIER
    lw $t0, RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET($t0)
    sw $t0, 0($sp)                                # Place Method* at bottom of stack.
    sw $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
    addiu  $sp, $sp, -ARG_SLOT_SIZE               # reserve argument slots on the stack
    .cfi_adjust_cfa_offset ARG_SLOT_SIZE
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly). Restoration assumes non-moving GC.
     * Does not include rSUSPEND or rSELF
     * callee-save: $s2-$s8 + $gp + $ra, 9 total + 2 words padding + 1 word to hold Method*
     * Clobbers $t0 and $sp
     * Allocates ARG_SLOT_SIZE bytes at the bottom of the stack for arg slots.
     * Reserves FRAME_SIZE_REFS_ONLY_CALLEE_SAVE + ARG_SLOT_SIZE bytes on the stack
     */
.macro SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, -48
    .cfi_adjust_cfa_offset 48

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_REFS_ONLY_CALLEE_SAVE != 48)
#error "REFS_ONLY_CALLEE_SAVE_FRAME(MIPS) size not as expected."
#endif

    sw     $ra, 44($sp)
    .cfi_rel_offset 31, 44
    sw     $s8, 40($sp)
    .cfi_rel_offset 30, 40
    sw     $gp, 36($sp)
    .cfi_rel_offset 28, 36
    sw     $s7, 32($sp)
    .cfi_rel_offset 23, 32
    sw     $s6, 28($sp)
    .cfi_rel_offset 22, 28
    sw     $s5, 24($sp)
    .cfi_rel_offset 21, 24
    sw     $s4, 20($sp)
    .cfi_rel_offset 20, 20
    sw     $s3, 16($sp)
    .cfi_rel_offset 19, 16
    sw     $s2, 12($sp)
    .cfi_rel_offset 18, 12
    # 2 words for alignment and bottom word will hold Method*

    lw $t0, %got(_ZN3art7Runtime9instance_E)($gp)
    lw $t0, 0($t0)
    THIS_LOAD_REQUIRES_READ_BARRIER
    lw $t0, RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET($t0)
    sw $t0, 0($sp)                                # Place Method* at bottom of stack.
    sw $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
    addiu  $sp, $sp, -ARG_SLOT_SIZE               # reserve argument slots on the stack
    .cfi_adjust_cfa_offset ARG_SLOT_SIZE
.endm

.macro RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, ARG_SLOT_SIZE                # remove argument slots on the stack
    .cfi_adjust_cfa_offset -ARG_SLOT_SIZE
    lw     $ra, 44($sp)
    .cfi_restore 31
    lw     $s8, 40($sp)
    .cfi_restore 30
    lw     $gp, 36($sp)
    .cfi_restore 28
    lw     $s7, 32($sp)
    .cfi_restore 23
    lw     $s6, 28($sp)
    .cfi_restore 22
    lw     $s5, 24($sp)
    .cfi_restore 21
    lw     $s4, 20($sp)
    .cfi_restore 20
    lw     $s3, 16($sp)
    .cfi_restore 19
    lw     $s2, 12($sp)
    .cfi_restore 18
    addiu  $sp, $sp, 48
    .cfi_adjust_cfa_offset -48
.endm

.macro RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    jalr   $zero, $ra
    nop
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs).
     * callee-save: $a1-$a3, $s2-$s8 + $gp + $ra, 12 total + 3 words padding + method*
     */
.macro SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_REGISTERS_ONLY
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
    sw     $a3, 24($sp)
    .cfi_rel_offset 7, 24
    sw     $a2, 20($sp)
    .cfi_rel_offset 6, 20
    sw     $a1, 16($sp)
    .cfi_rel_offset 5, 16
    # bottom will hold Method*
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs). Restoration assumes non-moving GC.
     * callee-save: $a1-$a3, $s2-$s8 + $gp + $ra, 12 total + 3 words padding + method*
     * Clobbers $t0 and $sp
     * Allocates ARG_SLOT_SIZE bytes at the bottom of the stack for arg slots.
     * Reserves FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE + ARG_SLOT_SIZE bytes on the stack
     */
.macro SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_REGISTERS_ONLY
    lw $t0, %got(_ZN3art7Runtime9instance_E)($gp)
    lw $t0, 0($t0)
    THIS_LOAD_REQUIRES_READ_BARRIER
    lw $t0, RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET($t0)
    sw $t0, 0($sp)                                # Place Method* at bottom of stack.
    sw $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
    addiu  $sp, $sp, -ARG_SLOT_SIZE               # reserve argument slots on the stack
    .cfi_adjust_cfa_offset ARG_SLOT_SIZE
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs). Restoration assumes non-moving GC.
     * callee-save: $a1-$a3, $s2-$s8 + $gp + $ra, 12 total + 3 words padding + method*
     * Clobbers $sp
     * Use $a0 as the Method* and loads it into bottom of stack.
     * Allocates ARG_SLOT_SIZE bytes at the bottom of the stack for arg slots.
     * Reserves FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE + ARG_SLOT_SIZE bytes on the stack
     */
.macro SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_A0
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_REGISTERS_ONLY
    sw $a0, 0($sp)                                # Place Method* at bottom of stack.
    sw $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
    addiu  $sp, $sp, -ARG_SLOT_SIZE               # reserve argument slots on the stack
    .cfi_adjust_cfa_offset ARG_SLOT_SIZE
.endm

.macro RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    addiu  $sp, $sp, ARG_SLOT_SIZE                # remove argument slots on the stack
    .cfi_adjust_cfa_offset -ARG_SLOT_SIZE
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
    lw     $a3, 24($sp)
    .cfi_restore 7
    lw     $a2, 20($sp)
    .cfi_restore 6
    lw     $a1, 16($sp)
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
    la      $t9, artDeliverPendingExceptionFromCode
    jalr    $zero, $t9                   # artDeliverPendingExceptionFromCode(Thread*)
    move    $a0, rSELF                   # pass Thread::Current
.endm

.macro RETURN_IF_NO_EXCEPTION
    lw     $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    bnez   $t0, 1f                       # success if no exception is pending
    nop
    jalr   $zero, $ra
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_ZERO
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    bnez   $v0, 1f                       # success?
    nop
    jalr   $zero, $ra                    # return on success
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME
    beqz   $v0, 1f                       # success?
    nop
    jalr   $zero, $ra                    # return on success
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

    /*
     * On entry $a0 is uint32_t* gprs_ and $a1 is uint32_t* fprs_
     * FIXME: just guessing about the shape of the jmpbuf.  Where will pc be?
     */
ENTRY art_quick_do_long_jump
    LDu  $f0,  $f1,   0*8, $a1, $t1
    LDu  $f2,  $f3,   1*8, $a1, $t1
    LDu  $f4,  $f5,   2*8, $a1, $t1
    LDu  $f6,  $f7,   3*8, $a1, $t1
    LDu  $f8,  $f9,   4*8, $a1, $t1
    LDu  $f10, $f11,  5*8, $a1, $t1
    LDu  $f12, $f13,  6*8, $a1, $t1
    LDu  $f14, $f15,  7*8, $a1, $t1
    LDu  $f16, $f17,  8*8, $a1, $t1
    LDu  $f18, $f19,  9*8, $a1, $t1
    LDu  $f20, $f21, 10*8, $a1, $t1
    LDu  $f22, $f23, 11*8, $a1, $t1
    LDu  $f24, $f25, 12*8, $a1, $t1
    LDu  $f26, $f27, 13*8, $a1, $t1
    LDu  $f28, $f29, 14*8, $a1, $t1
    LDu  $f30, $f31, 15*8, $a1, $t1

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
    jalr    $zero, $ra          # do long jump
    move    $v1, $zero
END art_quick_do_long_jump

    /*
     * Called by managed code, saves most registers (forms basis of long jump context) and passes
     * the bottom of the stack. artDeliverExceptionFromCode will place the callee save Method* at
     * the bottom of the thread. On entry a0 holds Throwable*
     */
ENTRY art_quick_deliver_exception
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artDeliverExceptionFromCode
    jalr $zero, $t9                 # artDeliverExceptionFromCode(Throwable*, Thread*)
    move $a1, rSELF                 # pass Thread::Current
END art_quick_deliver_exception

    /*
     * Called by managed code to create and deliver a NullPointerException
     */
    .extern artThrowNullPointerExceptionFromCode
ENTRY art_quick_throw_null_pointer_exception
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artThrowNullPointerExceptionFromCode
    jalr $zero, $t9                 # artThrowNullPointerExceptionFromCode(Thread*)
    move $a0, rSELF                 # pass Thread::Current
END art_quick_throw_null_pointer_exception

    /*
     * Called by managed code to create and deliver an ArithmeticException
     */
    .extern artThrowDivZeroFromCode
ENTRY art_quick_throw_div_zero
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artThrowDivZeroFromCode
    jalr $zero, $t9                 # artThrowDivZeroFromCode(Thread*)
    move $a0, rSELF                 # pass Thread::Current
END art_quick_throw_div_zero

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException
     */
    .extern artThrowArrayBoundsFromCode
ENTRY art_quick_throw_array_bounds
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artThrowArrayBoundsFromCode
    jalr $zero, $t9                 # artThrowArrayBoundsFromCode(index, limit, Thread*)
    move $a2, rSELF                 # pass Thread::Current
END art_quick_throw_array_bounds

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
    .extern artThrowStackOverflowFromCode
ENTRY art_quick_throw_stack_overflow
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artThrowStackOverflowFromCode
    jalr $zero, $t9                 # artThrowStackOverflowFromCode(Thread*)
    move $a0, rSELF                 # pass Thread::Current
END art_quick_throw_stack_overflow

    /*
     * Called by managed code to create and deliver a NoSuchMethodError.
     */
    .extern artThrowNoSuchMethodFromCode
ENTRY art_quick_throw_no_such_method
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artThrowNoSuchMethodFromCode
    jalr $zero, $t9                 # artThrowNoSuchMethodFromCode(method_idx, Thread*)
    move $a1, rSELF                 # pass Thread::Current
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
     * If unsuccessful, the helper will return null/null. There will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
.macro INVOKE_TRAMPOLINE c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME  # save callee saves in case allocation triggers GC
    lw    $a2, FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE+ARG_SLOT_SIZE($sp)    # pass caller Method*
    addiu $t0, $sp, ARG_SLOT_SIZE         # save $sp (remove arg slots)
    move  $a3, rSELF                      # pass Thread::Current
    jal   \cxx_name                       # (method_idx, this, caller, Thread*, $sp)
    sw    $t0, 16($sp)                    # pass $sp
    move  $a0, $v0                        # save target Method*
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    beqz  $v0, 1f
    move  $t9, $v1                        # save $v0->code_
    jalr  $zero, $t9
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
     *   a1 = argument array or null for no argument methods
     *   a2 = size of argument array in bytes
     *   a3 = (managed) thread pointer
     *   [sp + 16] = JValue* result
     *   [sp + 20] = shorty
     */
ENTRY art_quick_invoke_stub
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
    addiu $t0, $a2, 4           # create space for method pointer in frame.
    subu  $t0, $sp, $t0         # reserve & align *stack* to 16 bytes:
    srl   $t0, $t0, 4           # native calling convention only aligns to 8B,
    sll   $sp, $t0, 4           # so we have to ensure ART 16B alignment ourselves.
    addiu $a0, $sp, 4           # pass stack pointer + method ptr as dest for memcpy
    jal   memcpy                # (dest, src, bytes)
    addiu $sp, $sp, -16         # make space for argument slots for memcpy
    addiu $sp, $sp, 16          # restore stack after memcpy
    lw    $a0, 16($fp)          # restore method*
    lw    $a1, 4($sp)           # copy arg value for a1
    lw    $a2, 8($sp)           # copy arg value for a2
    lw    $a3, 12($sp)          # copy arg value for a3
    lw    $t9, ART_METHOD_QUICK_CODE_OFFSET_32($a0)  # get pointer to the code
    jalr  $t9                   # call the method
    sw    $zero, 0($sp)         # store null for method* at bottom of frame
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
    jalr  $zero, $ra
    sw    $v1, 4($t0)           # store the other half of the result
1:
    SDu   $f0, $f1, 0, $t0, $t1 # store floating point result
    jalr  $zero, $ra
    nop
END art_quick_invoke_stub

    /*
     * Entry from managed code that calls artHandleFillArrayDataFromCode and delivers exception on
     * failure.
     */
    .extern artHandleFillArrayDataFromCode
ENTRY art_quick_handle_fill_data
    lw     $a2, 0($sp)                    # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case exception allocation triggers GC
    jal    artHandleFillArrayDataFromCode # (payload offset, Array*, method, Thread*)
    move   $a3, rSELF                     # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_handle_fill_data

    /*
     * Entry from managed code that calls artLockObjectFromCode, may block for GC.
     */
    .extern artLockObjectFromCode
ENTRY art_quick_lock_object
    beqz    $a0, .Lart_quick_throw_null_pointer_exception_gp_set
    nop
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME     # save callee saves in case we block
    jal     artLockObjectFromCode         # (Object* obj, Thread*)
    move    $a1, rSELF                    # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_lock_object

    /*
     * Entry from managed code that calls artUnlockObjectFromCode and delivers exception on failure.
     */
    .extern artUnlockObjectFromCode
ENTRY art_quick_unlock_object
    beqz    $a0, .Lart_quick_throw_null_pointer_exception_gp_set
    nop
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME # save callee saves in case exception allocation triggers GC
    jal     artUnlockObjectFromCode   # (Object* obj, Thread*)
    move    $a1, rSELF                # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_unlock_object

    /*
     * Entry from managed code that calls artCheckCastFromCode and delivers exception on failure.
     */
    .extern artThrowClassCastException
ENTRY art_quick_check_cast
    addiu  $sp, $sp, -16
    .cfi_adjust_cfa_offset 16
    sw     $ra, 12($sp)
    .cfi_rel_offset 31, 12
    sw     $t9, 8($sp)
    sw     $a1, 4($sp)
    sw     $a0, 0($sp)
    jal    artIsAssignableFromCode
    addiu  $sp, $sp, -16             # reserve argument slots on the stack
    addiu  $sp, $sp, 16
    beqz   $v0, .Lthrow_class_cast_exception
    lw     $ra, 12($sp)
    jalr   $zero, $ra
    addiu  $sp, $sp, 16
    .cfi_adjust_cfa_offset -16
.Lthrow_class_cast_exception:
    lw     $t9, 8($sp)
    lw     $a1, 4($sp)
    lw     $a0, 0($sp)
    addiu  $sp, $sp, 16
    .cfi_adjust_cfa_offset -16
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    la   $t9, artThrowClassCastException
    jalr $zero, $t9                 # artThrowClassCastException (Class*, Class*, Thread*)
    move $a2, rSELF                 # pass Thread::Current
END art_quick_check_cast

    /*
     * Entry from managed code for array put operations of objects where the value being stored
     * needs to be checked for compatibility.
     * a0 = array, a1 = index, a2 = value
     */
ENTRY art_quick_aput_obj_with_null_and_bound_check
    bnez    $a0, .Lart_quick_aput_obj_with_bound_check_gp_set
    nop
    b .Lart_quick_throw_null_pointer_exception_gp_set
    nop
END art_quick_aput_obj_with_null_and_bound_check

ENTRY art_quick_aput_obj_with_bound_check
    lw $t0, MIRROR_ARRAY_LENGTH_OFFSET($a0)
    sltu $t1, $a1, $t0
    bnez $t1, .Lart_quick_aput_obj_gp_set
    nop
    move $a0, $a1
    b .Lart_quick_throw_array_bounds_gp_set
    move $a1, $t0
END art_quick_aput_obj_with_bound_check

ENTRY art_quick_aput_obj
    beqz $a2, .Ldo_aput_null
    nop
    lw $t0, MIRROR_OBJECT_CLASS_OFFSET($a0)
    lw $t1, MIRROR_OBJECT_CLASS_OFFSET($a2)
    lw $t0, MIRROR_CLASS_COMPONENT_TYPE_OFFSET($t0)
    bne $t1, $t0, .Lcheck_assignability  # value's type == array's component type - trivial assignability
    nop
.Ldo_aput:
    sll $a1, $a1, 2
    add $t0, $a0, $a1
    sw  $a2, MIRROR_OBJECT_ARRAY_DATA_OFFSET($t0)
    lw  $t0, THREAD_CARD_TABLE_OFFSET(rSELF)
    srl $t1, $a0, 7
    add $t1, $t1, $t0
    sb  $t0, ($t1)
    jalr $zero, $ra
    nop
.Ldo_aput_null:
    sll $a1, $a1, 2
    add $t0, $a0, $a1
    sw  $a2, MIRROR_OBJECT_ARRAY_DATA_OFFSET($t0)
    jalr $zero, $ra
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
    addiu $sp, $sp, -16     # reserve argument slots on the stack
    addiu $sp, $sp, 16
    lw     $ra, 28($sp)
    lw     $t9, 12($sp)
    lw     $a2, 8($sp)
    lw     $a1, 4($sp)
    lw     $a0, 0($sp)
    addiu  $sp, 32
    .cfi_adjust_cfa_offset -32
    bnez   $v0, .Ldo_aput
    nop
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    move $a1, $a2
    la   $t9, artThrowArrayStoreException
    jalr $zero, $t9                 # artThrowArrayStoreException(Class*, Class*, Thread*)
    move $a2, rSELF                 # pass Thread::Current
END art_quick_aput_obj

    /*
     * Called by managed code to resolve a static field and load a boolean primitive value.
     */
    .extern artGetBooleanStaticFromCode
ENTRY art_quick_get_boolean_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetBooleanStaticFromCode   # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_boolean_static
    /*
     * Called by managed code to resolve a static field and load a byte primitive value.
     */
    .extern artGetByteStaticFromCode
ENTRY art_quick_get_byte_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetByteStaticFromCode      # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_byte_static

    /*
     * Called by managed code to resolve a static field and load a char primitive value.
     */
    .extern artGetCharStaticFromCode
ENTRY art_quick_get_char_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetCharStaticFromCode      # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_char_static
    /*
     * Called by managed code to resolve a static field and load a short primitive value.
     */
    .extern artGetShortStaticFromCode
ENTRY art_quick_get_short_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetShortStaticFromCode     # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_short_static

    /*
     * Called by managed code to resolve a static field and load a 32-bit primitive value.
     */
    .extern artGet32StaticFromCode
ENTRY art_quick_get32_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGet32StaticFromCode        # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get32_static

    /*
     * Called by managed code to resolve a static field and load a 64-bit primitive value.
     */
    .extern artGet64StaticFromCode
ENTRY art_quick_get64_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGet64StaticFromCode        # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get64_static

    /*
     * Called by managed code to resolve a static field and load an object reference.
     */
    .extern artGetObjStaticFromCode
ENTRY art_quick_get_obj_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetObjStaticFromCode       # (uint32_t field_idx, const Method* referrer, Thread*)
    move   $a2, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_obj_static

    /*
     * Called by managed code to resolve an instance field and load a boolean primitive value.
     */
    .extern artGetBooleanInstanceFromCode
ENTRY art_quick_get_boolean_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetBooleanInstanceFromCode # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_boolean_instance
    /*
     * Called by managed code to resolve an instance field and load a byte primitive value.
     */
    .extern artGetByteInstanceFromCode
ENTRY art_quick_get_byte_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetByteInstanceFromCode    # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_byte_instance

    /*
     * Called by managed code to resolve an instance field and load a char primitive value.
     */
    .extern artGetCharInstanceFromCode
ENTRY art_quick_get_char_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetCharInstanceFromCode    # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_char_instance
    /*
     * Called by managed code to resolve an instance field and load a short primitive value.
     */
    .extern artGetShortInstanceFromCode
ENTRY art_quick_get_short_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetShortInstanceFromCode   # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_short_instance

    /*
     * Called by managed code to resolve an instance field and load a 32-bit primitive value.
     */
    .extern artGet32InstanceFromCode
ENTRY art_quick_get32_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGet32InstanceFromCode      # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get32_instance

    /*
     * Called by managed code to resolve an instance field and load a 64-bit primitive value.
     */
    .extern artGet64InstanceFromCode
ENTRY art_quick_get64_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGet64InstanceFromCode      # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get64_instance

    /*
     * Called by managed code to resolve an instance field and load an object reference.
     */
    .extern artGetObjInstanceFromCode
ENTRY art_quick_get_obj_instance
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artGetObjInstanceFromCode     # (field_idx, Object*, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_NO_EXCEPTION
END art_quick_get_obj_instance

    /*
     * Called by managed code to resolve a static field and store a 8-bit primitive value.
     */
    .extern artSet8StaticFromCode
ENTRY art_quick_set8_static
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet8StaticFromCode         # (field_idx, new_val, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set8_static

    /*
     * Called by managed code to resolve a static field and store a 16-bit primitive value.
     */
    .extern artSet16StaticFromCode
ENTRY art_quick_set16_static
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet16StaticFromCode        # (field_idx, new_val, referrer, Thread*, $sp)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set16_static

    /*
     * Called by managed code to resolve a static field and store a 32-bit primitive value.
     */
    .extern artSet32StaticFromCode
ENTRY art_quick_set32_static
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet32StaticFromCode        # (field_idx, new_val, referrer, Thread*)
    move   $a3, rSELF                    # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set32_static

    /*
     * Called by managed code to resolve a static field and store a 64-bit primitive value.
     */
    .extern artSet64StaticFromCode
ENTRY art_quick_set64_static
    lw     $a1, 0($sp)                   # pass referrer's Method*
                                         # 64 bit new_val is in a2:a3 pair
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet64StaticFromCode        # (field_idx, referrer, new_val, Thread*)
    sw     rSELF, 16($sp)                # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set64_static

    /*
     * Called by managed code to resolve a static field and store an object reference.
     */
    .extern artSetObjStaticFromCode
ENTRY art_quick_set_obj_static
    lw     $a2, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    move   $a3, rSELF                    # pass Thread::Current
    jal    artSetObjStaticFromCode       # (field_idx, new_val, referrer, Thread*)
    RETURN_IF_ZERO
END art_quick_set_obj_static

    /*
     * Called by managed code to resolve an instance field and store a 8-bit primitive value.
     */
    .extern artSet8InstanceFromCode
ENTRY art_quick_set8_instance
    lw     $a3, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet8InstanceFromCode       # (field_idx, Object*, new_val, referrer, Thread*)
    sw     rSELF, 16($sp)                # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set8_instance

    /*
     * Called by managed code to resolve an instance field and store a 16-bit primitive value.
     */
    .extern artSet16InstanceFromCode
ENTRY art_quick_set16_instance
    lw     $a3, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet16InstanceFromCode      # (field_idx, Object*, new_val, referrer, Thread*)
    sw     rSELF, 16($sp)                # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set16_instance

    /*
     * Called by managed code to resolve an instance field and store a 32-bit primitive value.
     */
    .extern artSet32InstanceFromCode
ENTRY art_quick_set32_instance
    lw     $a3, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSet32InstanceFromCode      # (field_idx, Object*, new_val, referrer, Thread*)
    sw     rSELF, 16($sp)                # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set32_instance

    /*
     * Called by managed code to resolve an instance field and store a 64-bit primitive value.
     */
    .extern artSet64InstanceFromCode
ENTRY art_quick_set64_instance
    lw     $t1, 0($sp)                   # load referrer's Method*
                                         # 64 bit new_val is in a2:a3 pair
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    sw     rSELF, 20($sp)                # pass Thread::Current
    jal    artSet64InstanceFromCode      # (field_idx, Object*, new_val, referrer, Thread*)
    sw     $t1, 16($sp)                  # pass referrer's Method*
    RETURN_IF_ZERO
END art_quick_set64_instance

    /*
     * Called by managed code to resolve an instance field and store an object reference.
     */
    .extern artSetObjInstanceFromCode
ENTRY art_quick_set_obj_instance
    lw     $a3, 0($sp)                   # pass referrer's Method*
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME    # save callee saves in case of GC
    jal    artSetObjInstanceFromCode     # (field_idx, Object*, new_val, referrer, Thread*)
    sw     rSELF, 16($sp)                # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_set_obj_instance

// Macro to facilitate adding new allocation entrypoints.
.macro ONE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME # save callee saves in case of GC
    jal     \entrypoint
    move    $a1, rSELF                # pass Thread::Current
    \return
END \name
.endm

.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME # save callee saves in case of GC
    jal     \entrypoint
    move    $a2, rSELF                # pass Thread::Current
    \return
END \name
.endm

.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME # save callee saves in case of GC
    jal     \entrypoint
    move    $a3, rSELF                # pass Thread::Current
    \return
END \name
.endm

.macro FOUR_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME # save callee saves in case of GC
    jal     \entrypoint
    sw      rSELF, 16($sp)            # pass Thread::Current
    \return
END \name
.endm

// Generate the allocation entrypoints for each allocator.
GENERATE_ALL_ALLOC_ENTRYPOINTS

    /*
     * Entry from managed code to resolve a string, this stub will allocate a String and deliver an
     * exception on error. On success the String is returned. A0 holds the string index. The fast
     * path check for hit in strings cache has already been performed.
     */
ONE_ARG_DOWNCALL art_quick_resolve_string, artResolveStringFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER

    /*
     * Entry from managed code when uninitialized static storage, this stub will run the class
     * initializer and deliver the exception on error. On success the static storage base is
     * returned.
     */
ONE_ARG_DOWNCALL art_quick_initialize_static_storage, artInitializeStaticStorageFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER

    /*
     * Entry from managed code when dex cache misses for a type_idx.
     */
ONE_ARG_DOWNCALL art_quick_initialize_type, artInitializeTypeFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER

    /*
     * Entry from managed code when type_idx needs to be checked for access and dex cache may also
     * miss.
     */
ONE_ARG_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode, RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER

    /*
     * Called by managed code when the value in rSUSPEND has been decremented to 0.
     */
    .extern artTestSuspendFromCode
ENTRY art_quick_test_suspend
    lh     $a0, THREAD_FLAGS_OFFSET(rSELF)
    bnez   $a0, 1f
    addiu  rSUSPEND, $zero, SUSPEND_CHECK_INTERVAL   # reset rSUSPEND to SUSPEND_CHECK_INTERVAL
    jalr   $zero, $ra
    nop
1:
    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME          # save callee saves for stack crawl
    jal    artTestSuspendFromCode              # (Thread*)
    move   $a0, rSELF
    RESTORE_REFS_ONLY_CALLEE_SAVE_FRAME_AND_RETURN
END art_quick_test_suspend

    /*
     * Called by managed code that is attempting to call a method on a proxy class. On entry
     * r0 holds the proxy method; r1, r2 and r3 may contain arguments.
     */
    .extern artQuickProxyInvokeHandler
ENTRY art_quick_proxy_invoke_handler
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_A0
    move    $a2, rSELF                  # pass Thread::Current
    jal     artQuickProxyInvokeHandler  # (Method* proxy method, receiver, Thread*, SP)
    addiu   $a3, $sp, ARG_SLOT_SIZE     # pass $sp (remove arg slots)
    lw      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    bnez    $t0, 1f
    # don't care if $v0 and/or $v1 are modified, when exception branch taken
    MTD     $v0, $v1, $f0, $f1          # move float value to return value
    jalr    $zero, $ra
    nop
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict. t0 is a hidden argument that holds the target method's
     * dex method index.
     */
ENTRY art_quick_imt_conflict_trampoline
    lw      $a0, 0($sp)            # load caller Method*
    lw      $a0, ART_METHOD_DEX_CACHE_METHODS_OFFSET($a0)  # load dex_cache_resolved_methods
    sll     $t0, 2                 # convert target method offset to bytes
    add     $a0, $t0               # get address of target method
    lw      $a0, MIRROR_OBJECT_ARRAY_DATA_OFFSET($a0)  # load the target method
    la      $t9, art_quick_invoke_interface_trampoline
    jalr    $zero, $t9
END art_quick_imt_conflict_trampoline

    .extern artQuickResolutionTrampoline
ENTRY art_quick_resolution_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    move    $a2, rSELF                    # pass Thread::Current
    jal     artQuickResolutionTrampoline  # (Method* called, receiver, Thread*, SP)
    addiu   $a3, $sp, ARG_SLOT_SIZE       # pass $sp (remove arg slots)
    beqz    $v0, 1f
    lw      $a0, ARG_SLOT_SIZE($sp)       # load resolved method to $a0
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    move    $t9, $v0               # code pointer must be in $t9 to generate the global pointer
    jalr    $zero, $t9             # tail call to method
    nop
1:
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_resolution_trampoline

    .extern artQuickGenericJniTrampoline
    .extern artQuickGenericJniEndTrampoline
ENTRY art_quick_generic_jni_trampoline
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME_WITH_METHOD_IN_A0
    move    $s8, $sp               # save $sp to $s8
    move    $s3, $gp               # save $gp to $s3

    # prepare for call to artQuickGenericJniTrampoline(Thread*, SP)
    move    $a0, rSELF                     # pass Thread::Current
    addiu   $a1, $sp, ARG_SLOT_SIZE        # save $sp (remove arg slots)
    jal     artQuickGenericJniTrampoline   # (Thread*, SP)
    addiu   $sp, $sp, -5120                # reserve space on the stack

    # The C call will have registered the complete save-frame on success.
    # The result of the call is:
    # v0: ptr to native code, 0 on error.
    # v1: ptr to the bottom of the used area of the alloca, can restore stack till here.
    beq     $v0, $zero, 1f         # check entry error
    move    $t9, $v0               # save the code ptr
    move    $sp, $v1               # release part of the alloca

    # Load parameters from stack into registers
    lw      $a0,   0($sp)
    lw      $a1,   4($sp)
    lw      $a2,   8($sp)

    # Load FPRs the same as GPRs. Look at BuildNativeCallFrameStateMachine.
    jalr    $t9                    # native call
    lw      $a3,  12($sp)
    addiu   $sp, $sp, 16           # remove arg slots

    move    $gp, $s3               # restore $gp from $s3

    # result sign extension is handled in C code
    # prepare for call to artQuickGenericJniEndTrampoline(Thread*, result, result_f)
    move    $a0, rSELF             # pass Thread::Current
    move    $a2, $v0               # pass result
    move    $a3, $v1
    addiu   $sp, $sp, -24          # reserve arg slots
    jal     artQuickGenericJniEndTrampoline
    s.d     $f0, 16($sp)           # pass result_f

    lw      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    bne     $t0, $zero, 1f         # check for pending exceptions

    move    $sp, $s8               # tear down the alloca

    # tear dpown the callee-save frame
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME

    MTD     $v0, $v1, $f0, $f1     # move float value to return value
    jalr    $zero, $ra
    nop

1:
    lw $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)
    # This will create a new save-all frame, required by the runtime.
    DELIVER_PENDING_EXCEPTION
END art_quick_generic_jni_trampoline

    .extern artQuickToInterpreterBridge
ENTRY art_quick_to_interpreter_bridge
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    move    $a1, rSELF                          # pass Thread::Current
    jal     artQuickToInterpreterBridge         # (Method* method, Thread*, SP)
    addiu   $a2, $sp, ARG_SLOT_SIZE             # pass $sp (remove arg slots)
    lw      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    bnez    $t0, 1f
    # don't care if $v0 and/or $v1 are modified, when exception branch taken
    MTD     $v0, $v1, $f0, $f1                  # move float value to return value
    jalr    $zero, $ra
    nop
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
    .extern artInstrumentationMethodEntryFromCode
    .extern artInstrumentationMethodExitFromCode
ENTRY art_quick_instrumentation_entry
    SETUP_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    sw       $a0, 28($sp)   # save arg0 in free arg slot
    move     $a3, $ra       # pass $ra
    jal      artInstrumentationMethodEntryFromCode  # (Method*, Object*, Thread*, LR)
    move     $a2, rSELF     # pass Thread::Current
    move     $t9, $v0       # $t9 holds reference to code
    lw       $a0, 28($sp)   # restore arg0 from free arg slot
    RESTORE_REFS_AND_ARGS_CALLEE_SAVE_FRAME
    jalr     $t9            # call method
    nop
END art_quick_instrumentation_entry
    /* intentional fallthrough */
    .global art_quick_instrumentation_exit
art_quick_instrumentation_exit:
    .cfi_startproc
    addiu    $t9, $ra, 4    # put current address into $t9 to rebuild $gp
    .cpload  $t9
    move     $ra, $zero     # link register is to here, so clobber with 0 for later checks

    SETUP_REFS_ONLY_CALLEE_SAVE_FRAME
    addiu    $sp, $sp, -16  # allocate temp storage on the stack
    .cfi_adjust_cfa_offset 16
    sw       $v0, ARG_SLOT_SIZE+12($sp)
    .cfi_rel_offset 2, ARG_SLOT_SIZE+12
    sw       $v1, ARG_SLOT_SIZE+8($sp)
    .cfi_rel_offset 3, ARG_SLOT_SIZE+8
    s.d      $f0, ARG_SLOT_SIZE($sp)
    s.d      $f0, 16($sp)   # pass fpr result
    move     $a2, $v0       # pass gpr result
    move     $a3, $v1
    addiu    $a1, $sp, ARG_SLOT_SIZE+16   # pass $sp (remove arg slots and temp storage)
    jal      artInstrumentationMethodExitFromCode  # (Thread*, SP, gpr_res, fpr_res)
    move     $a0, rSELF     # pass Thread::Current
    move     $t9, $v0       # set aside returned link register
    move     $ra, $v1       # set link register for deoptimization
    lw       $v0, ARG_SLOT_SIZE+12($sp)   # restore return values
    lw       $v1, ARG_SLOT_SIZE+8($sp)
    l.d      $f0, ARG_SLOT_SIZE($sp)
    jalr     $zero, $t9     # return
    addiu    $sp, $sp, ARG_SLOT_SIZE+FRAME_SIZE_REFS_ONLY_CALLEE_SAVE+16  # restore stack 
    .cfi_adjust_cfa_offset -(ARG_SLOT_SIZE+FRAME_SIZE_REFS_ONLY_CALLEE_SAVE+16)
END art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimize
ENTRY art_quick_deoptimize
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    jal      artDeoptimize  # artDeoptimize(Thread*)
                            # Returns caller method's frame size.
    move     $a0, rSELF     # pass Thread::current
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
ENTRY_NO_GP art_quick_shl_long
    /* shl-long vAA, vBB, vCC */
    sll     $v0, $a0, $a2                    #  rlo<- alo << (shift&31)
    not     $v1, $a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     $a0, 1
    srl     $a0, $v1                         #  alo<- alo >> (32-(shift&31))
    sll     $v1, $a1, $a2                    #  rhi<- ahi << (shift&31)
    andi    $a2, 0x20                        #  shift< shift & 0x20
    beqz    $a2, 1f
    or      $v1, $a0                         #  rhi<- rhi | alo

    move    $v1, $v0                         #  rhi<- rlo (if shift&0x20)
    move    $v0, $zero                       #  rlo<- 0 (if shift&0x20)

1:  jalr    $zero, $ra
    nop
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
ENTRY_NO_GP art_quick_shr_long
    sra     $v1, $a1, $a2                    #  rhi<- ahi >> (shift&31)
    srl     $v0, $a0, $a2                    #  rlo<- alo >> (shift&31)
    sra     $a3, $a1, 31                     #  $a3<- sign(ah)
    not     $a0, $a2                         #  alo<- 31-shift (shift is 5b)
    sll     $a1, 1
    sll     $a1, $a0                         #  ahi<- ahi << (32-(shift&31))
    andi    $a2, 0x20                        #  shift & 0x20
    beqz    $a2, 1f
    or      $v0, $a1                         #  rlo<- rlo | ahi

    move    $v0, $v1                         #  rlo<- rhi (if shift&0x20)
    move    $v1, $a3                         #  rhi<- sign(ahi) (if shift&0x20)

1:  jalr    $zero, $ra
    nop
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
ENTRY_NO_GP art_quick_ushr_long
    srl     $v1, $a1, $a2                    #  rhi<- ahi >> (shift&31)
    srl     $v0, $a0, $a2                    #  rlo<- alo >> (shift&31)
    not     $a0, $a2                         #  alo<- 31-shift (shift is 5b)
    sll     $a1, 1
    sll     $a1, $a0                         #  ahi<- ahi << (32-(shift&31))
    andi    $a2, 0x20                        #  shift & 0x20
    beqz    $a2, 1f
    or      $v0, $a1                         #  rlo<- rlo | ahi

    move    $v0, $v1                         #  rlo<- rhi (if shift&0x20)
    move    $v1, $zero                       #  rhi<- 0 (if shift&0x20)

1:  jalr    $zero, $ra
    nop
END art_quick_ushr_long

UNIMPLEMENTED art_quick_indexof
UNIMPLEMENTED art_quick_string_compareto
