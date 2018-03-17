/*
 * Copyright (C) 2017 The Android Open Source Project
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

// This file is special-purpose for cases where you want a stack context. Most users should use
// Context::Create().

#include "context.h"

#ifndef ART_RUNTIME_ARCH_CONTEXT_INL_H_
#define ART_RUNTIME_ARCH_CONTEXT_INL_H_

#if defined(__arm__)
#include "arm/context_arm.h"
#define RUNTIME_CONTEXT_TYPE arm::ArmContext
#elif defined(__aarch64__)
#include "arm64/context_arm64.h"
#define RUNTIME_CONTEXT_TYPE arm64::Arm64Context
#elif defined(__mips__) && !defined(__LP64__)
#include "mips/context_mips.h"
#define RUNTIME_CONTEXT_TYPE mips::MipsContext
#elif defined(__mips__) && defined(__LP64__)
#include "mips64/context_mips64.h"
#define RUNTIME_CONTEXT_TYPE mips64::Mips64Context
#elif defined(__i386__)
#include "x86/context_x86.h"
#define RUNTIME_CONTEXT_TYPE x86::X86Context
#elif defined(__x86_64__)
#include "x86_64/context_x86_64.h"
#define RUNTIME_CONTEXT_TYPE x86_64::X86_64Context
#else
#error unimplemented
#endif

namespace art {

using RuntimeContextType = RUNTIME_CONTEXT_TYPE;

}  // namespace art

#undef RUNTIME_CONTEXT_TYPE

#endif  // ART_RUNTIME_ARCH_CONTEXT_INL_H_
