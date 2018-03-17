/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_ARM_ASM_SUPPORT_ARM_H_
#define ART_RUNTIME_ARCH_ARM_ASM_SUPPORT_ARM_H_

#include "asm_support.h"

#define FRAME_SIZE_SAVE_ALL_CALLEE_SAVES 112
#define FRAME_SIZE_SAVE_REFS_ONLY 32
#define FRAME_SIZE_SAVE_REFS_AND_ARGS 112
#define FRAME_SIZE_SAVE_EVERYTHING 192

// The offset from the art_quick_read_barrier_mark_introspection (used for field
// loads with 32-bit LDR) to the entrypoint for field loads with 16-bit LDR,
// i.e. art_quick_read_barrier_mark_introspection_narrow.
#define BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_ENTRYPOINT_OFFSET 0x20
// The offsets from art_quick_read_barrier_mark_introspection to the GC root entrypoints,
// i.e. art_quick_read_barrier_mark_introspection_gc_roots_{wide,narrow}.
#define BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_ENTRYPOINT_OFFSET 0x80
#define BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_ENTRYPOINT_OFFSET 0xc0
// The offset from art_quick_read_barrier_mark_introspection to the array switch cases,
// i.e. art_quick_read_barrier_mark_introspection_arrays.
#define BAKER_MARK_INTROSPECTION_ARRAY_SWITCH_OFFSET 0x100

// The offset of the reference load LDR from the return address in LR for field loads.
#ifdef USE_HEAP_POISONING
#define BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET -8
#define BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET -4
#else
#define BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET -4
#define BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET -2
#endif
// The offset of the reference load LDR from the return address in LR for array loads.
#ifdef USE_HEAP_POISONING
#define BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET -8
#else
#define BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET -4
#endif
// The offset of the reference load LDR from the return address in LR for GC root loads.
#define BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_OFFSET -8
#define BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_OFFSET -6

// Flag for enabling R4 optimization in arm runtime
// #define ARM_R4_SUSPEND_FLAG

#endif  // ART_RUNTIME_ARCH_ARM_ASM_SUPPORT_ARM_H_
