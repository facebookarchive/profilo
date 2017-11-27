/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "asm_support_x86.S"

    /*
     * Jni dlsym lookup stub.
     */
DEFINE_FUNCTION art_jni_dlsym_lookup_stub
    subl LITERAL(4), %esp         // align stack
    CFI_ADJUST_CFA_OFFSET(4)
    SETUP_GOT                     // pushes ebx
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call PLT_SYMBOL(artFindNativeMethod)  // (Thread*)
    addl LITERAL(4), %esp         // remove argument
    CFI_ADJUST_CFA_OFFSET(-4)
    UNDO_SETUP_GOT                // pop ebx
    addl LITERAL(4), %esp         // remove padding
    CFI_ADJUST_CFA_OFFSET(-4)
    testl %eax, %eax         // check if returned method code is null
    jz .Lno_native_code_found     // if null, jump to return to handle
    jmp *%eax                     // otherwise, tail call to intended method
.Lno_native_code_found:
    ret
END_FUNCTION art_jni_dlsym_lookup_stub
