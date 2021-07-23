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
  //
  // This code implements the linker PLT hooking trampoline contract.
  // Namely:
  //   hook = push_hook(.L_hook_id, <return_address>);
  //   hook(<original arguments>);
  //   ret = pop_hook();
  //   longjump(ret); // with return from hook()
  //
  // Further, the trampoline must be entirely position-independent, with
  // the relevant function pointers in trampoline_data.
  //

  // Save the frame pointer, then store the stack's location in it. We will
  // need this to get the return address to pass to the push_hook() call.
  "pushq   %rbp\n"
  "movq    %rsp,  %rbp\n"

  // Save  all the registers used to pass arguments
  "subq    $192,  %rsp\n"
  "movupd  %xmm0, 176(%rsp)\n"
  "movupd  %xmm1, 160(%rsp)\n"
  "movupd  %xmm2, 144(%rsp)\n"
  "movupd  %xmm3, 128(%rsp)\n"
  "movupd  %xmm4, 112(%rsp)\n"
  "movupd  %xmm5,  96(%rsp)\n"
  "movupd  %xmm6,  80(%rsp)\n"
  "movupd  %xmm7,  64(%rsp)\n"
  "movq    %rdi,   56(%rsp)\n"
  "movq    %rax,   48(%rsp)\n"
  "movq    %rdx,   40(%rsp)\n"
  "movq    %rsi,   32(%rsp)\n"
  "movq    %rcx,   24(%rsp)\n"
  "movq    %r8,    16(%rsp)\n"
  "movq    %r9,     8(%rsp)\n"
  "movq    %r10,     (%rsp)\n"


  // Store the value of .L_hook_id into the first argument register
  "movq    .L_hook_id(%rip), %rdi\n"

  // Copy the return address that was on the top of the stack when we
  // entered the trampoline into the 2nd argument register
  "movq  8(%rbp), %rsi\n"

  // Call push_hook(.L_hook_id, <return address>). The stack needs to be
  // aligned to 16 bytes (e.g. %rsp % 16 == 0) before each call instruction
  // to comply with the ABI, maintain that if modifying this code.
  "call    *.L_push_hook_stack(%rip)\n"
  // now %rax contains the hook to call

  // copy the hook address to a scratch register
  "movq %rax, %r11\n"

  // Restore the argument registers
  "movupd  176(%rsp), %xmm0\n"
  "movupd  160(%rsp), %xmm1\n"
  "movupd  144(%rsp), %xmm2\n"
  "movupd  128(%rsp), %xmm3\n"
  "movupd  112(%rsp), %xmm4\n"
  "movupd   96(%rsp), %xmm5\n"
  "movupd   80(%rsp), %xmm6\n"
  "movupd   64(%rsp), %xmm7\n"
  "movq     56(%rsp), %rdi\n"
  "movq     48(%rsp), %rax\n"
  "movq     40(%rsp), %rdx\n"
  "movq     32(%rsp), %rsi\n"
  "movq     24(%rsp), %rcx\n"
  "movq     16(%rsp), %r8\n"
  "movq      8(%rsp), %r9\n"
  "movq       (%rsp), %r10\n"
  "addq    $192,      %rsp\n"

  // Now put the stack back to where it was when we entered the trampoline.
  // This ensures any stack-based arguments will be where the hooked function
  // expects them to be
  "movq    %rbp, %rsp\n"
  "popq    %rbp\n"

  // Erase the caller's return address. The call below will replace it with ours - we
  // saved it with push_hook() above
  "addq    $8, %rsp\n"

  // Call the hook
  "call    *%r11\n"

  // Save the return values from the hook
  "subq    $48,   %rsp\n"
  "movupd  %xmm0, 32(%rsp)\n"
  "movupd  %xmm1, 16(%rsp)\n"
  "movq    %rax,   8(%rsp)\n"
  "movq    %rdx,    (%rsp)\n"

  // Call pop_hook() to get the original return address back
  "call    *.L_pop_hook_stack(%rip)\n"
  // %rax is now the address to return to

  // Copy the return address to a scratch register
  "movq %rax, %r11\n"

  // Restore the return registers
  "movupd  32(%rsp), %xmm0\n"
  "movupd  16(%rsp), %xmm1\n"
  "movq    8(%rsp),  %rax\n"
  "movq     (%rsp),  %rdx\n"
  "addq    $48,      %rsp\n"

  // Return to the address given to us by pop_hook()
  "pushq   %r11\n"
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
