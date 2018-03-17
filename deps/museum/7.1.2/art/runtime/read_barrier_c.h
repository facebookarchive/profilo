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

#ifndef ART_RUNTIME_READ_BARRIER_C_H_
#define ART_RUNTIME_READ_BARRIER_C_H_

// This is a C (not C++) header file and is in a separate file (from
// globals.h) because asm_support.h is a C header file and can't
// include globals.h.

// Uncomment one of the following two and the two fields in
// Object.java (libcore) to enable baker, brooks (unimplemented), or
// table-lookup read barriers.

#ifdef ART_USE_READ_BARRIER
#if ART_READ_BARRIER_TYPE_IS_BAKER
#define USE_BAKER_READ_BARRIER
#elif ART_READ_BARRIER_TYPE_IS_BROOKS
#define USE_BROOKS_READ_BARRIER
#elif ART_READ_BARRIER_TYPE_IS_TABLELOOKUP
#define USE_TABLE_LOOKUP_READ_BARRIER
#else
#error "ART read barrier type must be set"
#endif
#endif  // ART_USE_READ_BARRIER

#ifdef ART_HEAP_POISONING
#define USE_HEAP_POISONING
#endif

#if defined(USE_BAKER_READ_BARRIER) || defined(USE_BROOKS_READ_BARRIER)
#define USE_BAKER_OR_BROOKS_READ_BARRIER
#endif

#if defined(USE_BAKER_READ_BARRIER) || defined(USE_BROOKS_READ_BARRIER) || defined(USE_TABLE_LOOKUP_READ_BARRIER)
#define USE_READ_BARRIER
#endif

#if defined(USE_BAKER_READ_BARRIER) && defined(USE_BROOKS_READ_BARRIER)
#error "Only one of Baker or Brooks can be enabled at a time."
#endif

#endif  // ART_RUNTIME_READ_BARRIER_C_H_
