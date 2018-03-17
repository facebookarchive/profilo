/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_MIPS64_ASM_SUPPORT_MIPS64_H_
#define ART_RUNTIME_ARCH_MIPS64_ASM_SUPPORT_MIPS64_H_

#include "asm_support.h"

// 64 ($f24-$f31) + 64 ($s0-$s7) + 8 ($gp) + 8 ($s8) + 8 ($ra) + 1x8 bytes padding
#define FRAME_SIZE_SAVE_ALL_CALLEE_SAVE 160
// 48 ($s2-$s7) + 8 ($gp) + 8 ($s8) + 8 ($ra) + 1x8 bytes padding
#define FRAME_SIZE_REFS_ONLY_CALLEE_SAVE 80
// $f12-$f19, $a1-$a7, $s2-$s7 + $gp + $s8 + $ra, 16 total + 1x8 bytes padding + method*
#define FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE 208

#endif  // ART_RUNTIME_ARCH_MIPS64_ASM_SUPPORT_MIPS64_H_
