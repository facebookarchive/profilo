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
    // x86 concerns:
    //   There's no eip-relative addressing, so loading the values from our
    //   trampoline_data area involves the following pattern:
    //
    //     call pic_trampoline
    //     pic_trampoline:
    //     // the above emits a relative `call +0`
    //     popl %eax
    //     // %eax is now address of pic_trampoline
    //     addl (address_of_thing_we_need - pic_trampoline), %eax
    //     // the calculation above is known at link time and gets substituted
    //     // for an immediate value. %eax is now address_of_thing_we_need
    //
    //   `call` and `ret` implicitly use the stack. `call` pushes the return
    //   address and `ret` pops the address to return to.
    //   Therefore, in order to retain control after we call hook(<args>),
    //   we must modify the value on the stack. We preserve this value in the
    //   initial push_hook() call.
    //
    //   Floating-point returns happen via the FPU stack, in particular the
    //   top register, st0. We perform full 80-bit copies from the stack after
    //   calling hook() via the relevant FPU instructions.
    //
    //   Linux toolchain specific (gcc expects this, maybe clang too):
    //   The stack must be 16-byte aligned *at the `call` instruction*.
    //   This is counter-intuitive because it means that `%esp mod 16 == 12`
    //   on the callee end due to the implicit push of a return address
    //   as part of `call`.
    //

    // Stack alignment mod 16: 12 bytes

    // Set up a new frame because we'll be using some stack space
    // for parameter passing.
    "pushl  %ebp\n"
    "movl   %esp, %ebp\n"
    // Stack alignment mod 16: 8 bytes

    // PIC code to access push_hook_stack and L_chained.
    "call   .L_pic_trampoline_1\n"
    ".L_pic_trampoline_1:\n"
    "popl   %ecx\n" // ecx = address of this exact instruction
    // Stack alignment mod 16: 8 bytes (call + pop cancel each other)

    // second param for push_hook_stack == return address == the value at the
    // top of the stack when we entered the trampoline.
    "pushl  4(%ebp)\n"
    // Stack alignment mod 16: 4 bytes

    "addl   $(.L_hook_id - .L_pic_trampoline_1), %ecx\n"
    // ecx =  address of .L_hook_id

    "movl   (%ecx), %eax\n" // eax = .L_hook_id
    "pushl  %eax\n " // first argument
    // Stack alignment mod 16: 0 bytes

    // Convert the address of .L_hook_id into the address of .L_push_hook_stack
    "addl  $(.L_push_hook_stack - .L_hook_id), %ecx\n"
    "movl  (%ecx), %eax\n"

    // Stack alignment mod 16: 0 bytes
    "call   *%eax\n"
    // %eax now contains the hook we need to call

    // We're done with our frame, restore old frame before calling the hook.
    "movl   %ebp, %esp\n"
    "popl   %ebp\n"
    // Stack alignment mod 16: 12 bytes

    // Remove the return address that's already on the stack, we saved it in
    // our `push_hook` call.
    "addl   $4, %esp\n"
    // Stack alignment mod 16: 0 bytes

    // Call hook.
    "call   *%eax\n"
    // Stack alignment mod 16: 0 bytes (call + ret cancel each other)

    // save eax & edx, the return values from the hook func
    "pushl  %eax\n"
    // Stack alignment mod 16: 12 bytes
    "pushl  %edx\n"
    // Stack alignment mod 16: 8 bytes
    // save st0 which is used for floating point returns
    "subl   $10, %esp\n"
    // Stack alignment mod 16: 14 bytes
    "fstpt  (%esp)\n"

    // Set up temporary frame
    "pushl  %ebp\n"
    "movl   %esp, %ebp\n"
    // Stack alignment mod 16: 10 bytes

    // align stack on a 16-byte boundary
    "andl   $0xfffffff0, %esp\n"
    // Stack alignment mod 16: 0 bytes

    // Another PIC trampoline, this time for pop_hook_stack
    "call   .L_pic_trampoline_3\n"
    ".L_pic_trampoline_3:\n"
    "popl   %ecx\n"
    // Stack alignment mod 16: 0 bytes

    "addl   $(.L_pop_hook_stack - .L_pic_trampoline_3), %ecx\n"
    "movl   (%ecx), %eax\n"

    // Stack alignment mod 16: 0 bytes
    // Call pop function.
    "call   *%eax\n"
    // eax is now the address we need to return to, move it somewhere else
    "movl   %eax, %ecx\n"

    // Tear down temporary frame
    "movl   %ebp, %esp\n"
    "popl   %ebp\n"
    // Stack alignment mod 16: 14 bytes

    // Restore return result from hook as the result of the trampoline
    "fldt   (%esp)\n"
    "addl   $10, %esp\n"
    // Stack alignment mod 16: 8 bytes
    "popl   %edx\n"
    // Stack alignment mod 16: 12 bytes
    "popl   %eax\n"
    // Stack alignment mod 16: 0 bytes

    "pushl   %ecx\n" // restore return address, it got removed by the hook's ret
    // Stack alignment mod 16: 12 bytes
    "ret\n"

    "trampoline_data:"
    ".global trampoline_data;"
    ".L_push_hook_stack:"
    ".word 0; .word 0;"
    ".L_pop_hook_stack:"
    ".word 0; .word 0;"
    ".L_hook_id:"
    ".word 0; .word 0;"
  );
}
