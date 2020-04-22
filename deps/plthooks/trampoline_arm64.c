/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

__attribute__((naked))
void trampoline_template() {
  asm(
    "sub   sp, sp, #0xd0\n"

    // Save argument registers
    "stp   q0, q1, [sp, #0xb0]\n"
    "stp   q2, q3, [sp, #0x90]\n"
    "stp   q4, q5, [sp, #0x70]\n"
    "stp   q6, q7, [sp, #0x50]\n"
    "stp   x0, x1, [sp, #0x40]\n"
    "stp   x2, x3, [sp, #0x30]\n"
    "stp   x4, x5, [sp, #0x20]\n"
    "stp   x6, x7, [sp, #0x10]\n"
    // Save indirect return value register
    "str   x8, [sp]\n"

    "ldr   x0, .L_hook_id\n"
    "mov   x1, lr\n"
    "ldr   x16, .L_push_hook_stack\n"
    "blr   x16\n"
    "mov   x16, x0\n"

    // Restore argument registers
    "ldr   x8, [sp]\n"
    "ldp   x6, x7, [sp, #0x10]\n"
    "ldp   x4, x5, [sp, #0x20]\n"
    "ldp   x2, x3, [sp, #0x30]\n"
    "ldp   x0, x1, [sp, #0x40]\n"
    "ldp   q6, q7, [sp, #0x50]\n"
    "ldp   q4, q5, [sp, #0x70]\n"
    "ldp   q2, q3, [sp, #0x90]\n"
    "ldp   q0, q1, [sp, #0xb0]\n"

    // Tear down frame, so that called function sees the stack exactly as it's
    // expected (so that e.g. arguments on the stack have the correct offsets)
    "add   sp, sp, #0xd0\n"

    // Call hooked function
    "blr   x16\n"

    // Save registers used for return values. Aggregates up to 16 bytes might
    // be returned in x0-x1. Homogenous floating point aggregates up to 4
    // elements might be returned in q0-q3. The indirect return value register
    // x8 does *not* need to be preserved; it's caller-saved, so our caller will
    // have taken care of it. See the AAPCS64 documentation [1] for details.
    // [1] https://github.com/ARM-software/abi-aa/blob/master/aapcs64/aapcs64.rst
    "sub   sp, sp, #0x50\n"
    "stp   q0, q1, [sp, #0x30]\n"
    "stp   q2, q3, [sp, #0x10]\n"
    "stp   x0, x1, [sp]\n"

    "ldr   x16, .L_pop_hook_stack\n"
    "blr   x16\n"
    "mov   lr, x0\n"

    "ldp   x0, x1, [sp]\n"
    "ldp   q2, q3, [sp, #0x10]\n"
    "ldp   q0, q1, [sp, #0x30]\n"
    "add   sp, sp, #0x50\n"
    "ret\n"

    "trampoline_data:"
    ".global trampoline_data;"
    ".L_push_hook_stack:"
    ".quad 0;"
    ".L_pop_hook_stack:"
    ".quad 0;"
    ".L_hook_id:"
    ".quad 0;"
  );
}
