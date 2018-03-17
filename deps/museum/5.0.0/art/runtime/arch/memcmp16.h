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

#ifndef ART_RUNTIME_ARCH_MEMCMP16_H_
#define ART_RUNTIME_ARCH_MEMCMP16_H_

#include <cstddef>
#include <cstdint>

// memcmp16 support.
//
// This can either be optimized assembly code, in which case we expect a function __memcmp16,
// or generic C support.
//
// In case of the generic support we declare two versions: one in this header file meant to be
// inlined, and a static version that assembly stubs can link against.
//
// In both cases, MemCmp16 is declared.

#if defined(__aarch64__) || defined(__arm__) || defined(__mips) || defined(__i386__) || defined(__x86_64__)

extern "C" uint32_t __memcmp16(const uint16_t* s0, const uint16_t* s1, size_t count);
#define MemCmp16 __memcmp16

#else

// This is the generic inlined version.
static inline int32_t MemCmp16(const uint16_t* s0, const uint16_t* s1, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (s0[i] != s1[i]) {
      return static_cast<int32_t>(s0[i]) - static_cast<int32_t>(s1[i]);
    }
  }
  return 0;
}

extern "C" int32_t memcmp16_generic_static(const uint16_t* s0, const uint16_t* s1, size_t count);
#endif

namespace art {

namespace testing {

// A version that is exposed and relatively "close to the metal," so that memcmp16_test can do
// some reasonable testing. Without this, as __memcmp16 is hidden, the test cannot access the
// implementation.
int32_t MemCmp16Testing(const uint16_t* s0, const uint16_t* s1, size_t count);

}

}  // namespace art

#endif  // ART_RUNTIME_ARCH_MEMCMP16_H_
