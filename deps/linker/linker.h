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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the library, call to this function is expected before any other
 * function is invoked.
 * On failure returns non-zero value and errno is set appropriately.
 */
int
linker_initialize();

/**
 * Populates the global cache of shared library data which is used to install
 * hooks.
*/
int
refresh_shared_libs();

/**
 * Overwrites GOT entry for particular function with provided address,
 * effectively hijacking all invocations of given function in given library.
 * In case of failure non-zero value is returned and errno is set.
 * ##### NOTE: USE ABSOLUTE PATHS FOR LIBRARY NAMES #####
 */
int
hook_plt_method(void* dlhandle, const char* libname, const char* name, void* hook);

/**
 * Overwrites GOT entry for a set of functions & hooks.
 * hook_specs is treated as inout, and hook_result will be updated
 * to reflect success/failure of any given operation.
 */
typedef struct _plt_hook_spec {
  const char* lib_name;
  const char* fn_name;
  void* hook_fn;
  int dlopen_result;
  int hook_result;

#if defined(__cplusplus)
  _plt_hook_spec(const char* lname, const char* fname, void* hfn)
    : lib_name(lname), fn_name(fname), hook_fn(hfn), dlopen_result(0), hook_result(0)
  {}
#endif //defined(__cplusplus)
} plt_hook_spec;

/**
 * Overwrites GOT entry for particular function with provided address,
 * effectively hijacking all invocations of given function in given library.
 * In case of failure non-zero value is returned and errno is set.
 */
int
hook_single_lib(void* dlhandle, char const* libname, plt_hook_spec* specs,
    size_t num_specs);

int
hook_all_libs(plt_hook_spec* specs, size_t num_specs,
    bool (*allowHookingLib)(char const* libname, void* data), void* data);


/**
 * Looks up the previous PLT entry for a given hook and library. Here be
 * dragons; you probably want CALL_PREV(..) instead.
 *
 * hook - Code address of an instruction inside the hooking (*not* hooked)
 *        function. AKA, pass this the current instruction pointer.
 * return_address - Return address for the current frame, used to look up which
 *                  library's PLT entry is needed.
 *
 * Returns: void* - code address of the function previously pointed to by the
 *                  appropriate entry of the appropriate PLT
 */
void*
lookup_prev_plt_method(void* hook, void* return_address);

/**
 * Returns the address of the instruction immediately after this call. Used
 * by CALL_PREV.
 */
__attribute__((noinline))
void*
get_pc_thunk();

#ifdef __cplusplus
} // extern "C"
#endif
