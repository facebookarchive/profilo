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

/* Assumptions:
 *
 * ARMv8-a, AArch64
 */

#ifndef ART_RUNTIME_ARCH_ARM64_MEMCMP16_ARM64_S_
#define ART_RUNTIME_ARCH_ARM64_MEMCMP16_ARM64_S_

#include "asm_support_arm64.S"

/* Parameters and result.  */
#define src1        x0
#define src2        x1
#define limit       x2
#define result      x0

/* Internal variables.  */
#define data1       x3
#define data1w      w3
#define data2       x4
#define data2w      w4
#define has_nul     x5
#define diff        x6
#define endloop     x7
#define tmp1        x8
#define tmp2        x9
#define tmp3        x10
#define limit_wd    x12
#define mask        x13

// WARNING: If you change this code to use x14 and x15, you must also change
//          art_quick_string_compareto, which relies on these temps being unused.

ENTRY __memcmp16
  cbz     limit, .Lret0
  lsl     limit, limit, #1  /* Half-words to bytes.  */
  eor     tmp1, src1, src2
  tst     tmp1, #7
  b.ne    .Lmisaligned8
  ands    tmp1, src1, #7
  b.ne    .Lmutual_align
  add     limit_wd, limit, #7
  lsr     limit_wd, limit_wd, #3
  /* Start of performance-critical section  -- one 64B cache line.  */
.Lloop_aligned:
  ldr     data1, [src1], #8
  ldr     data2, [src2], #8
.Lstart_realigned:
  subs    limit_wd, limit_wd, #1
  eor     diff, data1, data2  /* Non-zero if differences found.  */
  csinv   endloop, diff, xzr, ne  /* Last Dword or differences.  */
  cbz     endloop, .Lloop_aligned
  /* End of performance-critical section  -- one 64B cache line.  */

  /* Not reached the limit, must have found a diff.  */
  cbnz    limit_wd, .Lnot_limit

  /* Limit % 8 == 0 => all bytes significant.  */
  ands    limit, limit, #7
  b.eq    .Lnot_limit

  lsl     limit, limit, #3  /* Bits -> bytes.  */
  mov     mask, #~0
  lsl     mask, mask, limit
  bic     data1, data1, mask
  bic     data2, data2, mask

.Lnot_limit:

  // Swap the byte order of diff. Exact reverse is not important, as we only need to detect
  // the half-word.
  rev     diff, diff
  // The most significant bit of DIFF marks the least significant bit of change between DATA1/2
  clz     diff, diff
  // Mask off 0xF to have shift amount. Why does ARM64 not have BIC with immediate?!?!
  bfi     diff, xzr, #0, #4
  // Create a 16b mask
  mov     mask, #0xFFFF
  // Shift to the right half-word.
  lsr     data1, data1, diff
  lsr     data2, data2, diff
  // Mask the lowest half-word.
  and     data1, data1, mask
  and     data2, data2, mask
  // Compute difference.
  sub     result, data1, data2
  ret

.Lmutual_align:
  /* Sources are mutually aligned, but are not currently at an
     alignment boundary.  Round down the addresses and then mask off
     the bytes that precede the start point.  */
  bic     src1, src1, #7
  bic     src2, src2, #7
  add     limit, limit, tmp1  /* Adjust the limit for the extra.  */
  lsl     tmp1, tmp1, #3    /* Bytes beyond alignment -> bits.  */
  ldr     data1, [src1], #8
  neg     tmp1, tmp1    /* Bits to alignment -64.  */
  ldr     data2, [src2], #8
  mov     tmp2, #~0
  /* Little-endian.  Early bytes are at LSB.  */
  lsr     tmp2, tmp2, tmp1  /* Shift (tmp1 & 63).  */
  add     limit_wd, limit, #7
  orr     data1, data1, tmp2
  orr     data2, data2, tmp2
  lsr     limit_wd, limit_wd, #3
  b       .Lstart_realigned

.Lret0:
  mov     result, #0
  ret

  .p2align 6
.Lmisaligned8:
  sub     limit, limit, #1
1:
  /* Perhaps we can do better than this.  */
  ldrh    data1w, [src1], #2
  ldrh    data2w, [src2], #2
  subs    limit, limit, #2
  ccmp    data1w, data2w, #0, cs  /* NZCV = 0b0000.  */
  b.eq    1b
  sub     result, data1, data2
  ret
END __memcmp16

#endif  // ART_RUNTIME_ARCH_ARM64_MEMCMP16_ARM64_S_
