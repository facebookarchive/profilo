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
    "sub   sp, sp, #0x60\n"
    // Set up frame
    "stp   x29, x30, [sp, #0x50]\n"
    "add   x29, sp, #0x50\n"

    // Save argument registers
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

    "ldr   x8, [sp]\n"
    "ldp   x6, x7, [sp, #0x10]\n"
    "ldp   x4, x5, [sp, #0x20]\n"
    "ldp   x2, x3, [sp, #0x30]\n"
    "ldp   x0, x1, [sp, #0x40]\n"

    "blr   x16\n"

    "stp   x0, x1, [sp, #0x40]\n"
    "stp   x2, x3, [sp, #0x30]\n"
    "stp   x4, x5, [sp, #0x20]\n"
    "stp   x6, x7, [sp, #0x10]\n"
    "str   x8, [sp]\n"

    "ldr   x16, .L_pop_hook_stack\n"
    "blr   x16\n"
    "mov   lr, x0\n"

    "ldr   x8, [sp]\n"
    "ldp   x6, x7, [sp, #0x10]\n"
    "ldp   x4, x5, [sp, #0x20]\n"
    "ldp   x2, x3, [sp, #0x30]\n"
    "ldp   x0, x1, [sp, #0x40]\n"

    // Tear down frame
    "ldp  x29, x30, [sp, #0x50]\n"
    "add  sp, sp, #0x60\n"
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
