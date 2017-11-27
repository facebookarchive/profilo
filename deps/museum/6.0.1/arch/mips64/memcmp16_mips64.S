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

#ifndef ART_RUNTIME_ARCH_MIPS64_MEMCMP16_MIPS64_S_
#define ART_RUNTIME_ARCH_MIPS64_MEMCMP16_MIPS64_S_

#include "asm_support_mips64.S"

.set noreorder

// u4 __memcmp16(const u2*, const u2*, size_t);
ENTRY_NO_GP __memcmp16
  move  $t0, $zero
  move  $t1, $zero
  beqz  $a2, done       /* 0 length string */
  nop
  beq   $a0, $a1, done  /* addresses are identical */
  nop

1:
  lhu   $t0, 0($a0)
  lhu   $t1, 0($a1)
  bne   $t0, $t1, done
  nop
  daddu $a0, 2
  daddu $a1, 2
  dsubu $a2, 1
  bnez  $a2, 1b
  nop

done:
  dsubu $v0, $t0, $t1
  j     $ra
  nop
END __memcmp16

#endif  // ART_RUNTIME_ARCH_MIPS64_MEMCMP16_MIPS64_S_
