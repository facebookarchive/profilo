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
  // save registers we clobber (lr) and this particular hook's chained function
  // onto a TLS stack so that we can easily look up who CALL_PREV should jump to, and
  // clean up after ourselves register-wise, all while ensuring that we don't alter
  // the actual thread stack at all in order to make sure the hook function sees exactly
  // the parameters it's supposed to.
  // We intentionally clobber ip, it's an inter-procedure scratch register anyway.
  asm(
    "push  { r0 - r3 };" // AAPCS doesn't require preservation of r0 - r3 across calls, so save em temporarily
    "ldr   r0, .L_hook_id;" // store hook id
    "mov   r1, lr;" // save lr so we know where to go back to once this is all done
    "ldr   ip, .L_push_hook_stack;"
    "blx   ip;"
    "mov   ip, r0;" // return value saved, that's the hook we'll call
    "pop   { r0 - r3 };" // bring the hook's original parameters back

    "blx   ip;" // switches to ARM or Thumb mode appropriately since target is a register

    // now restore what we saved above
    "push  { r0 - r3 };"
    "ldr   ip, .L_pop_hook_stack;"
    "blx   ip;"
    "mov   lr, r0;" // return value from pop_hook_stack
    "pop   { r0 - r3 };"

    "bx    lr;" // finally, return to our caller

    "trampoline_data:"
    ".global trampoline_data;"
    ".L_push_hook_stack:"
    ".word 0;"
    ".L_pop_hook_stack:"
    ".word 0;"
    ".L_hook_id:"
    ".word 0;"
  );
}
