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

/* C++ header.
 *
 * This file encapsulates the C++ specific portion of the linker library.
 * This is necessary because we have some C source files which cannot make
 * use of the C++ code, but which require #include'ing linker.h nonetheless.
*/

#pragma once

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string>

#include <unordered_set>

#include "linker.h"

namespace linker {

class internal_exception : public virtual std::runtime_error {
  public:
    internal_exception(const char* what_arg) :
      std::runtime_error(what_arg) {}
};
} // namespace linker

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
 *
 * If we are unable to find the original ("previous") function, then we throw
 * a runtime error.
 */

static std::unordered_set<void*> prevMethodCache;

#define CALL_PREV(orig_method, ...)                                     \
  ({                                                                    \
    void* _hook = get_pc_thunk();                                       \
    void* _caller = __builtin_return_address(0);                        \
    void* _prev = lookup_prev_plt_method(_hook, _caller);               \
    if (_prev == nullptr) {                                             \
      char buf[1024]{};                                                 \
      Dl_info hook_info;                                                \
      Dl_info caller_info;                                              \
      dladdr(_hook, &hook_info);                                        \
      dladdr(_caller, &caller_info);                                    \
      snprintf(buf, sizeof(buf), "liblinker: could not find "           \
          "previous method.\n_hook.fname = %s, _hook.sname = %s, "      \
          "_hook.address = %p, _caller.fname = %s, "                    \
          "_caller.sname = %s, _caller.address = %p",                   \
          hook_info.dli_fname, hook_info.dli_sname, _hook,              \
          caller_info.dli_fname, caller_info.dli_sname, _caller);       \
      throw linker::internal_exception(buf);                            \
    }                                                                   \
    if (prevMethodCache.find(_prev) == prevMethodCache.cend()) {        \
      Dl_info info;                                                     \
      if (!dladdr(_prev, &info)) {                                      \
        char buf[128]{};                                                \
        snprintf(buf, sizeof(buf), "liblinker error: address %p of "    \
            "previous method is not within our address space", _prev);  \
        throw linker::internal_exception(buf);                          \
      }                                                                 \
      prevMethodCache.insert(_prev);                                    \
    }                                                                   \
    auto retvar = ((decltype((orig_method)))_prev)(__VA_ARGS__);        \
    retvar;                                                             \
  })
