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

#ifndef ART_RUNTIME_ARCH_MIPS_MEMCMP16_MIPS_S_
#define ART_RUNTIME_ARCH_MIPS_MEMCMP16_MIPS_S_

#include "asm_support_mips.S"

// u4 __memcmp16(const u2*, const u2*, size_t);
ENTRY __memcmp16
  li  $t0,0
  li  $t1,0
  beqz  $a2,done   /* 0 length string */
  beq $a0,$a1,done    /* strings are identical */

  /* Unoptimised... */
1:  lhu $t0,0($a0)
  lhu $t1,0($a1)
  addu  $a1,2
  bne $t0,$t1,done
  addu  $a0,2
  subu  $a2,1
  bnez  $a2,1b

done:
  subu  $v0,$t0,$t1
  j $ra
END __memcmp16

#endif  // ART_RUNTIME_ARCH_MIPS_MEMCMP16_MIPS_S_
