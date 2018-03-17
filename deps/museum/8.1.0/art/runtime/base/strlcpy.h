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

#ifndef ART_RUNTIME_BASE_STRLCPY_H_
#define ART_RUNTIME_BASE_STRLCPY_H_

#include <cstdio>
#include <cstring>

// Expose a simple implementation of strlcpy when we're not compiling against bionic. This is to
// make static analyzers happy not using strcpy.
//
// Bionic exposes this function, but the host glibc does not. Remove this shim when we compile
// against bionic on the host, also.

#if !defined(__BIONIC__) && !defined(__APPLE__)

static inline size_t strlcpy(char* dst, const char* src, size_t size) {
  // Extra-lazy implementation: this is only a host shim, and we don't have to call this often.
  return snprintf(dst, size, "%s", src);
}

#endif

#endif  // ART_RUNTIME_BASE_STRLCPY_H_
