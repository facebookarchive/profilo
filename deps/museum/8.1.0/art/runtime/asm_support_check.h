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

#include <museum/8.1.0/art/runtime/art_method.h>
#include <museum/8.1.0/art/runtime/base/bit_utils.h>
#include <museum/8.1.0/art/runtime/base/callee_save_type.h>
#include <museum/8.1.0/art/runtime/gc/accounting/card_table.h>
#include <museum/8.1.0/art/runtime/gc/allocator/rosalloc.h>
#include <museum/8.1.0/art/runtime/gc/heap.h>
#include <museum/8.1.0/art/runtime/jit/jit.h>
#include <museum/8.1.0/art/runtime/lock_word.h>
#include <museum/8.1.0/art/runtime/mirror/class.h>
#include <museum/8.1.0/art/runtime/mirror/dex_cache.h>
#include <museum/8.1.0/art/runtime/mirror/string.h>
#include <museum/8.1.0/art/runtime/utils/dex_cache_arrays_layout.h>
#include <museum/8.1.0/art/runtime/runtime.h>
#include <museum/8.1.0/art/runtime/stack.h>
#include <museum/8.1.0/art/runtime/thread.h>

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

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

static inline ASM_SUPPORT_CHECK_RETURN_TYPE CheckAsmSupportOffsetsAndSizes() {
#ifdef ASM_SUPPORT_CHECK_HEADER
  ASM_SUPPORT_CHECK_HEADER
#endif

#include <museum/8.1.0/art/runtime/asm_support.h>

#ifdef ASM_SUPPORT_CHECK_FOOTER
  ASM_SUPPORT_CHECK_FOOTER
#endif
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_ASM_SUPPORT_CHECK_H_
