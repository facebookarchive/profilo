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

#ifndef ART_RUNTIME_ARCH_ARM_MEMCMP16_ARM_S_
#define ART_RUNTIME_ARCH_ARM_MEMCMP16_ARM_S_

#include "asm_support_arm.S"

/*
 * Optimized memcmp16() for ARM9.
 * This would not be optimal on XScale or ARM11, where more prefetching
 * and use of pld will be needed.
 * The 2 major optimzations here are
 * (1) The main loop compares 16 bytes at a time
 * (2) The loads are scheduled in a way they won't stall
 */

ARM_ENTRY __memcmp16
        pld         [r0, #0]
        pld         [r1, #0]

        /* take of the case where length is nul or the buffers are the same */
        cmp         r0, r1
        cmpne       r2, #0
        moveq       r0, #0
        bxeq        lr

        /* since r0 hold the result, move the first source
         * pointer somewhere else
         */

        mov         r3, r0

         /* make sure we have at least 12 words, this simplify things below
          * and avoid some overhead for small blocks
          */

        cmp         r2, #12
        bpl         0f

        /* small blocks (less then 12 words) */
        pld         [r0, #32]
        pld         [r1, #32]

1:      ldrh        r0, [r3], #2
        ldrh        ip, [r1], #2
        subs        r0, r0, ip
        bxne        lr
        subs        r2, r2, #1
        bne         1b
        bx          lr


        /* save registers */
0:      stmfd       sp!, {r4, lr}
        .cfi_def_cfa_offset 8
        .cfi_rel_offset r4, 0
        .cfi_rel_offset lr, 4

        /* align first pointer to word boundary */
        tst         r3, #2
        beq         0f

        ldrh        r0, [r3], #2
        ldrh        ip, [r1], #2
        sub         r2, r2, #1
        subs        r0, r0, ip
        /* restore registers and return */
        ldmnefd     sp!, {r4, lr}
        bxne        lr


0:      /* here the first pointer is aligned, and we have at least 3 words
         * to process.
         */

        /* see if the pointers are congruent */
        eor         r0, r3, r1
        ands        r0, r0, #2
        bne         5f

        /* congruent case, 16 half-words per iteration
         * We need to make sure there are at least 16+2 words left
         * because we effectively read ahead one long word, and we could
         * read past the buffer (and segfault) if we're not careful.
         */

        ldr         ip, [r1]
        subs        r2, r2, #(16 + 2)
        bmi         1f

0:
        pld         [r3, #64]
        pld         [r1, #64]
        ldr         r0, [r3], #4
        ldr         lr, [r1, #4]!
        eors        r0, r0, ip
        ldreq       r0, [r3], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        ldreq       r0, [r3], #4
        ldreq       lr, [r1, #4]!
        eoreqs      r0, r0, ip
        ldreq       r0, [r3], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        ldreq       r0, [r3], #4
        ldreq       lr, [r1, #4]!
        eoreqs      r0, r0, ip
        ldreq       r0, [r3], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        ldreq       r0, [r3], #4
        ldreq       lr, [r1, #4]!
        eoreqs      r0, r0, ip
        ldreq       r0, [r3], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        bne         2f
        subs        r2, r2, #16
        bhs         0b

        /* do we have at least 2 words left? */
1:      adds        r2, r2, #(16 - 2 + 2)
        bmi         4f

        /* finish off 2 words at a time */
3:      ldr         r0, [r3], #4
        ldr         ip, [r1], #4
        eors        r0, r0, ip
        bne         2f
        subs        r2, r2, #2
        bhs         3b

        /* are we done? */
4:      adds        r2, r2, #2
        bne         8f
        /* restore registers and return */
        mov         r0, #0
        ldmfd       sp!, {r4, lr}
        bx          lr

2:      /* the last 2 words are different, restart them */
        ldrh        r0, [r3, #-4]
        ldrh        ip, [r1, #-4]
        subs        r0, r0, ip
        ldreqh      r0, [r3, #-2]
        ldreqh      ip, [r1, #-2]
        subeqs      r0, r0, ip
        /* restore registers and return */
        ldmfd       sp!, {r4, lr}
        bx          lr

        /* process the last few words */
8:      ldrh        r0, [r3], #2
        ldrh        ip, [r1], #2
        subs        r0, r0, ip
        bne         9f
        subs        r2, r2, #1
        bne         8b

9:      /* restore registers and return */
        ldmfd       sp!, {r4, lr}
        bx          lr


5:      /*************** non-congruent case ***************/

        /* align the unaligned pointer */
        bic         r1, r1, #3
        ldr         lr, [r1], #4
        sub         r2, r2, #8

6:
        pld         [r3, #64]
        pld         [r1, #64]
        mov         ip, lr, lsr #16
        ldr         lr, [r1], #4
        ldr         r0, [r3], #4
        orr         ip, ip, lr, lsl #16
        eors        r0, r0, ip
        moveq       ip, lr, lsr #16
        ldreq       lr, [r1], #4
        ldreq       r0, [r3], #4
        orreq       ip, ip, lr, lsl #16
        eoreqs      r0, r0, ip
        moveq       ip, lr, lsr #16
        ldreq       lr, [r1], #4
        ldreq       r0, [r3], #4
        orreq       ip, ip, lr, lsl #16
        eoreqs      r0, r0, ip
        moveq       ip, lr, lsr #16
        ldreq       lr, [r1], #4
        ldreq       r0, [r3], #4
        orreq       ip, ip, lr, lsl #16
        eoreqs      r0, r0, ip
        bne         7f
        subs        r2, r2, #8
        bhs         6b
        sub         r1, r1, #2
        /* are we done? */
        adds        r2, r2, #8
        moveq       r0, #0
        beq         9b
        /* finish off the remaining bytes */
        b           8b

7:      /* fix up the 2 pointers and fallthrough... */
        sub         r1, r1, #2
        b           2b
END __memcmp16


#endif  // ART_RUNTIME_ARCH_ARM_MEMCMP16_ARM_S_
