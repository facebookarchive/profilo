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

#include "asm_support_arm64.S"

    /*
     * Jni dlsym lookup stub.
     */
    .extern artFindNativeMethod

    // TODO: Add CFI directives.
ENTRY art_jni_dlsym_lookup_stub
  // spill regs.
  stp   x29, x30, [sp, #-16]!
  mov   x29, sp
  stp   d6, d7,   [sp, #-16]!
  stp   d4, d5,   [sp, #-16]!
  stp   d2, d3,   [sp, #-16]!
  stp   d0, d1,   [sp, #-16]!
  stp   x6, x7,   [sp, #-16]!
  stp   x4, x5,   [sp, #-16]!
  stp   x2, x3,   [sp, #-16]!
  stp   x0, x1,   [sp, #-16]!

  bl  artFindNativeMethod
  mov  x17, x0    // store result in scratch reg.

  // load spill regs.
  ldp   x0, x1,   [sp], #16
  ldp   x2, x3,   [sp], #16
  ldp   x4, x5,   [sp], #16
  ldp   x6, x7,   [sp], #16
  ldp   d0, d1,   [sp], #16
  ldp   d2, d3,   [sp], #16
  ldp   d4, d5,   [sp], #16
  ldp   d6, d7,   [sp], #16
  ldp   x29, x30, [sp], #16

  cbz   x17, 1f   // is method code null ?
  br    x17       // if non-null, tail call to method's code.

1:
  ret             // restore regs and return to caller to handle exception.
END art_jni_dlsym_lookup_stub
