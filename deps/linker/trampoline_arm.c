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
#if defined(TRAMP_ARM)
void trampoline_template_arm() {
#elif defined(TRAMP_THUMB)
void trampoline_template_thumb() {
#endif
  // save registers we clobber (lr, ip) and this particular hook's chained function
  // onto a TLS stack so that we can easily look up who CALL_PREV should jump to, and
  // clean up after ourselves register-wise, all while ensuring that we don't alter
  // the actual thread stack at all in order to make sure the hook function sees exactly
  // the parameters it's supposed to.
  asm(
    "push  { r0 - r3 };" // AAPCS doesn't require preservation of r0 - r3 across calls, so save em temporarily
    "ldr   r0, .L_chained;" // store chained function for easy lookup
    "mov   r2, ip;" // save ip so we can use it as our scratch register
    "ldr   ip, .L_push_hook_stack;"
    "mov   r1, lr;" // save lr so we know where to go back to once this is all done
    "blx   ip;"
    "pop   { r0 - r3 };" // bring the hook's original parameters back

    "ldr   ip, .L_hook;"
    "blx   ip;" // switches to ARM or Thumb mode appropriately since target is a register

    // now restore what we saved above
    // NOTE: pop_hook_stack returns a uint64 that is actually two uint32's packed together.
    // the AAPCS specifies that double-word fundamental types (aka, uint64_t) are placed in
    // r0 and r1, so we can simply pack our values, grab the registers, and be on our way
    "push  { r0 - r3 };"
    "ldr   ip, .L_pop_hook_stack;"
    "blx   ip;"
    "mov   lr, r0;"
    "mov   ip, r1;"
    "pop   { r0 - r3 };"

    "bx    lr;" // finally, return to our caller

#if defined(TRAMP_ARM)
    "trampoline_data_arm:"
    ".global trampoline_data_arm;"
#elif defined(TRAMP_THUMB)
    "trampoline_data_thumb:"
    ".global trampoline_data_thumb;"
#endif
    ".L_push_hook_stack:"
    ".word 0;"
    ".L_pop_hook_stack:"
    ".word 0;"
    ".L_hook:"
    ".word 0;"
    ".L_chained:"
    ".word 0;"
  );
}
