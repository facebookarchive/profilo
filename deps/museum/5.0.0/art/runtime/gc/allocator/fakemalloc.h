/**
 * Copyright 2018-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <museum/5.0.0/bionic/libc/stdlib.h>
#include <museum/5.0.0/bionic/libc/inttypes.h>

typedef void* mspace;

static inline size_t
mspace_usable_size(const void* mem) {
  abort();
}

static inline void*
mspace_malloc(mspace msp, size_t bytes) {
  abort();
}
