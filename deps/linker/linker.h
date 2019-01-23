/**
 * Copyright 2004-present, Facebook, Inc.
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

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Helper typedefs for conceptual separation in return values and parameters.
 */
typedef void* symbol;
typedef void** reloc;

/**
 * Guards initialization from above. Can be used to no-op calls to
 * linker_initialize by calling this with `false`.
 */
void
linker_set_enabled(int enabled);

/**
 * Initializes the library, call to this function is expected before any other
 * function is invoked.
 * This is additionally guarded by linker_set_enabled() above.
 *
 * On failure returns non-zero value and errno is set appropriately.
 */
int
linker_initialize();

/**
 * Populates the global cache of shared library data which is used to install
 * hooks. Returns 0 on success, nonzero on failure.
 */
int
refresh_shared_libs();

/**
 * Finds all linker relocations that point to the resolved symbol. relocs_out is an
 * array that gets filled with pointers to memory locations that point to sym.
 *
 * Returns the number of relocations written to relocs_out, or -1 on error (errno
 * will be set).
 *
 * Note: These are different than PLT relocations, and aren't used for PLT hooking.
 */
int
get_relocations(symbol sym, reloc* relocs_out, size_t relocs_out_len);

#ifdef __cplusplus
} // extern "C"
#endif
