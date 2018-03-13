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

#ifndef ART_RUNTIME_GLOBALS_H_
#define ART_RUNTIME_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>
#include "read_barrier_c.h"
#include "read_barrier_option.h"

namespace art {

static constexpr size_t KB = 1024;
static constexpr size_t MB = KB * KB;
static constexpr size_t GB = KB * KB * KB;

// Runtime sizes.
static constexpr size_t kBitsPerByte = 8;
static constexpr size_t kBitsPerByteLog2 = 3;
static constexpr int kBitsPerIntPtrT = sizeof(intptr_t) * kBitsPerByte;

// Required stack alignment
static constexpr size_t kStackAlignment = 16;

// System page size. We check this against sysconf(_SC_PAGE_SIZE) at runtime, but use a simple
// compile-time constant so the compiler can generate better code.
static constexpr int kPageSize = 4096;

// Required object alignment
static constexpr size_t kObjectAlignment = 8;
static constexpr size_t kLargeObjectAlignment = kPageSize;

// Whether or not this is a debug build. Useful in conditionals where NDEBUG isn't.
#if defined(NDEBUG)
static constexpr bool kIsDebugBuild = false;
#else
static constexpr bool kIsDebugBuild = true;
#endif

// Whether or not this is a target (vs host) build. Useful in conditionals where ART_TARGET isn't.
#if defined(ART_TARGET)
static constexpr bool kIsTargetBuild = true;
#else
static constexpr bool kIsTargetBuild = false;
#endif

#if defined(ART_USE_OPTIMIZING_COMPILER)
static constexpr bool kUseOptimizingCompiler = true;
#else
static constexpr bool kUseOptimizingCompiler = false;
#endif

// Garbage collector constants.
static constexpr bool kMovingCollector = true;
static constexpr bool kMarkCompactSupport = false && kMovingCollector;
// True if we allow moving classes.
static constexpr bool kMovingClasses = !kMarkCompactSupport;

// If true, the quick compiler embeds class pointers in the compiled
// code, if possible.
static constexpr bool kEmbedClassInCode = true;

#ifdef USE_BAKER_READ_BARRIER
static constexpr bool kUseBakerReadBarrier = true;
#else
static constexpr bool kUseBakerReadBarrier = false;
#endif

#ifdef USE_BROOKS_READ_BARRIER
static constexpr bool kUseBrooksReadBarrier = true;
#else
static constexpr bool kUseBrooksReadBarrier = false;
#endif

#ifdef USE_TABLE_LOOKUP_READ_BARRIER
static constexpr bool kUseTableLookupReadBarrier = true;
#else
static constexpr bool kUseTableLookupReadBarrier = false;
#endif

static constexpr bool kUseBakerOrBrooksReadBarrier = kUseBakerReadBarrier || kUseBrooksReadBarrier;
static constexpr bool kUseReadBarrier = kUseBakerReadBarrier || kUseBrooksReadBarrier ||
    kUseTableLookupReadBarrier;

// If true, references within the heap are poisoned (negated).
#ifdef ART_HEAP_POISONING
static constexpr bool kPoisonHeapReferences = true;
#else
static constexpr bool kPoisonHeapReferences = false;
#endif

// If true, enable the tlab allocator by default.
#ifdef ART_USE_TLAB
static constexpr bool kUseTlab = true;
#else
static constexpr bool kUseTlab = false;
#endif

// Kinds of tracing clocks.
enum class TraceClockSource {
  kThreadCpu,
  kWall,
  kDual,  // Both wall and thread CPU clocks.
};

#if defined(__linux__)
static constexpr TraceClockSource kDefaultTraceClockSource = TraceClockSource::kDual;
#else
static constexpr TraceClockSource kDefaultTraceClockSource = TraceClockSource::kWall;
#endif

static constexpr bool kDefaultMustRelocate = true;

static constexpr bool kArm32QuickCodeUseSoftFloat = false;

}  // namespace art

#endif  // ART_RUNTIME_GLOBALS_H_
