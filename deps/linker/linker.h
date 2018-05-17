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
typedef void* hook_func;
typedef void** reloc;

/**
 * Initializes the library, call to this function is expected before any other
 * function is invoked.
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

/**
 * Overwrites GOT entry for particular function with provided address,
 * effectively hijacking all invocations of given function in given library.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
hook_plt_method(const char* libname, const char* name, hook_func hook);

typedef struct _plt_hook_spec {
  const char* lib_name;
  const char* fn_name;
  hook_func hook_fn;
  int dlopen_result;
  int hook_result;

#if defined(__cplusplus)
  _plt_hook_spec(const char* lname, const char* fname, hook_func hfn)
    : lib_name(lname), fn_name(fname), hook_fn(hfn), dlopen_result(0), hook_result(0)
  {}
#endif //defined(__cplusplus)
} plt_hook_spec;

/**
 * Overwrites GOT entry for specified functions with provided addresses,
 * effectively hijacking all invocations of given function in given library.
 *
 * Returns the number of failures that occurred during hooking (0 for total success),
 * and increments plt_hook_spec::hook_result for each hook that succeeds. Note that
 * it is possible to have some, but not all, hooks fail. (Not finding a PLT entry in
 * a library is *not* counted as a failure.)
 */
int
hook_single_lib(char const* libname, plt_hook_spec* specs, size_t num_specs);

/**
 * Overwrites GOT entries for specified functions with provided addresses,
 * effectively hijacking all invocations of given function in given library.
 *
 * Returns the number of failures that occurred during hooking (0 for total success),
 * and increments plt_hook_spec::hook_result for each hook that succeeds. Note that
 * it is possible to have some, but not all, hooks fail. (Not finding a PLT entry in
 * a library is *not* counted as a failure.)
 */
int
hook_all_libs(plt_hook_spec* specs, size_t num_specs,
    bool (*allowHookingLib)(char const* libname, void* data), void* data);

/**
 * Calls the original (or at least, previous) method pointed to by the PLT.
 * Looks up PLT entries by hook *and* by library, since each library has its
 * own PLT and thus could have different entries.
 *
 * Takes as the first parameter the hook function itself, and then the args
 * as normal of the function. Returns the same type as the hooked function.
 *
 * If C99, a second parameter is required before the args describing the function
 * pointer type.
 *
 * Example:
 *   int write_hook(int fd, const void* buf, size_t count) {
 *     // do_some_hooky_stuff
 *
 *     ret = CALL_PREV(write_hook, fd, buf, count); // if C++
 *
 *     ret = CALL_PREV(write_hook,                  // if C99
 *                     int(*)(int, void const*, size_t),
 *                     fd, buf, count);
 *   }
 *
 * Aborts loudly if unable to find the previous function.
 */
#if defined(__cplusplus)
#define CALL_PREV(hook, ...) __CALL_PREV_IMPL(hook, decltype(&hook), __VA_ARGS__)
#else
#define CALL_PREV(hook, hook_sig, ...) __CALL_PREV_IMPL(hook, hook_sig, __VA_ARGS__)
#endif

#define __CALL_PREV_IMPL(hook, hook_sig, ...)                           \
  ({                                                                    \
    void* _caller = __builtin_return_address(0);                        \
    void* _prev = lookup_prev_plt_method(                               \
      (hook_func)hook, _caller, __FILE__, __LINE__);                    \
    ((hook_sig)_prev)(__VA_ARGS__);                                     \
  })
/**
 * Looks up the previous PLT entry for a given hook and library. Here be
 * dragons; you probably want CALL_PREV(..) instead.
 *
 * hook - Address of the currently-executing hook function.
 * return_address - Return address for the current frame, used to look up which
 *                  library's PLT entry is needed.
 * file - Filename of callsite, only used in case of error.
 * line - Line of callsite, only used in case of error.
 *
 * Returns: void* - code address of the function previously pointed to by the
 *                  appropriate entry of the appropriate PLT
 */
void*
lookup_prev_plt_method(hook_func hook, void* return_address, char const* file, int line);

#ifdef __cplusplus
} // extern "C"
#endif
