// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

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
 * Overwrites GOT entry for particular function with provided address,
 * effectively hijacking all invocations of given function in given library.
 * In case of failure non-zero value is returned and errno is set.
 */
int
hook_plt_method(void* dlhandle, const char* name, void* hook);

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

void
hook_plt_methods(plt_hook_spec* hook_specs, size_t num_hook_specs);


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

/**
 * Calls the original (or at least, previous) method pointed to by the PLT.
 * Looks up PLT entries by hook *and* by library, since each library has its
 * own PLT and thus could have different entries.
 *
 * Takes as the first parameter a pointer to the original function (used only
 * by the compiler, actual runtime pointer value is unused) and then the args
 * as normal of the function. Returns the same type as the hooked function.
 *
 * Example:
 *   int write_hook(int fd, const void* buf, size_t count) {
 *     // do_some_hooky_stuff
 *     return CALL_PREV(&write, fd, buf, count);
 *   }
 */
#define CALL_PREV(orig_method, ...)                                 \
  (((decltype(orig_method))lookup_prev_plt_method(                  \
    get_pc_thunk(),                                                 \
    __builtin_return_address(0)))(__VA_ARGS__))

#ifdef __cplusplus
}
#endif
