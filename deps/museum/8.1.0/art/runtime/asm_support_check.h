/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_ASM_SUPPORT_CHECK_H_
#define ART_RUNTIME_ASM_SUPPORT_CHECK_H_

#include "art_method.h"
#include "base/bit_utils.h"
#include "base/callee_save_type.h"
#include "gc/accounting/card_table.h"
#include "gc/allocator/rosalloc.h"
#include "gc/heap.h"
#include "jit/jit.h"
#include "lock_word.h"
#include "mirror/class.h"
#include "mirror/dex_cache.h"
#include "mirror/string.h"
#include "utils/dex_cache_arrays_layout.h"
#include "runtime.h"
#include "stack.h"
#include "thread.h"

#ifndef ADD_TEST_EQ
#define ADD_TEST_EQ(x, y) CHECK_EQ(x, y);
#endif

#ifndef ASM_SUPPORT_CHECK_RETURN_TYPE
#define ASM_SUPPORT_CHECK_RETURN_TYPE void
#endif

// Prepare for re-include of asm_support.h.
#ifdef ART_RUNTIME_ASM_SUPPORT_H_
#undef ART_RUNTIME_ASM_SUPPORT_H_
#endif

namespace art {

static inline ASM_SUPPORT_CHECK_RETURN_TYPE CheckAsmSupportOffsetsAndSizes() {
#ifdef ASM_SUPPORT_CHECK_HEADER
  ASM_SUPPORT_CHECK_HEADER
#endif

#include "asm_support.h"

#ifdef ASM_SUPPORT_CHECK_FOOTER
  ASM_SUPPORT_CHECK_FOOTER
#endif
}

}  // namespace art

#endif  // ART_RUNTIME_ASM_SUPPORT_CHECK_H_
