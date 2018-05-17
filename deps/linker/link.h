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

#if defined(__BIONIC__)

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Matching symbol table entry (const ElfNN_Sym *).  */
    RTLD_DL_SYMENT = 1,

    /* The object containing the address (struct link_map *).  */
    RTLD_DL_LINKMAP = 2
};

/**
 * Implementation of glibc's dladdr1.
 *
 * addr - Address to look up
 * info - Pointer to Dl_info struct to fill in
 * extra_info - Pointer to ElfW(Sym) const* that will be filled in
 * flags - only RTLD_DL_SYMENT supported
 *
 * Returns 1 on success, 0 on failure. dlerror will not be set.
 */
int dladdr1(void* addr, Dl_info* info, void** extra_info, int flags);

#ifdef __cplusplus
}
#endif

#endif // defined(__BIONIC__)
