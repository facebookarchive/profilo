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
typedef void* hook_func;


/**
 * This callback can supplied to hook_all_libs batch hooking method and gives
 * a library client control whether to hook a particular library or not.
 *
 * Returns true if the library <libname> should be hooked and false otherwise.
 */
typedef bool (*AllowHookingLibCallback)(
    char const* libname,
    char const* full_libname,
    void* data);

/**
 * Initializes the library, call to this function is expected before any other
 * function is invoked.
 *
 * On failure returns non-zero value and errno is set appropriately.
 */
int
plthooks_initialize();

/**
 * Overwrites GOT entry for particular function with provided address,
 * effectively hijacking all invocations of given function in given library.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
hook_plt_method(const char* libname, const char* name, hook_func hook);

typedef struct _plt_hook_spec {
  const char* fn_name;
  hook_func hook_fn;
  int hook_result;

#if defined(__cplusplus)
  _plt_hook_spec(const char* fname, hook_func hfn)
    : fn_name(fname), hook_fn(hfn), hook_result(0)
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
    bool (*allowHookingLib)(char const* libname, char const* full_libname, void* data), void* data);

// Counterparts to the all hook_* routines:

int
unhook_plt_method(const char* libname, const char* name, hook_func hook);

int
unhook_single_lib(char const* libname, plt_hook_spec* specs, size_t num_specs);

int
unhook_all_libs(plt_hook_spec* specs, size_t num_specs);

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
    void* _prev = get_previous_from_hook((void*) hook);                 \
    ((hook_sig)_prev)(__VA_ARGS__);                                     \
  })
/**
 * Looks up the previous PLT entry for a given hook and library. Here be
 * dragons; you probably want CALL_PREV(..) instead.
 *
 * Returns: void* - code address of the function previously pointed to by the
 *                  appropriate entry of the appropriate PLT
 */
void*
get_previous_from_hook(void* hook);

#ifdef __cplusplus
} // extern "C"
#endif
