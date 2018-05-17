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

#include <linker/link.h>
#include <linker/linker.h>
#include <linker/sharedlibs.h>
#include <linker/locks.h>
#include <linker/log_assert.h>

#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <cjni/log.h>
#include <sig_safe_write/sig_safe_write.h>
#include <sigmux.h>

#include <algorithm>
#include <cinttypes>
#include <map>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <functional>

#define PAGE_ALIGN(ptr, pagesize) (void*) (((uintptr_t) (ptr)) & ~((pagesize) - 1))

using namespace facebook::linker;

typedef void* prev_func;
typedef void* lib_base;

namespace {

struct pairhash {
  std::size_t operator()(std::pair<hook_func, lib_base> const& pair) const {
    // guts of boost::hash_combine. see https://stackoverflow.com/a/27952689 for more
#if __SIZEOF_SIZE_T__ == 4
    constexpr size_t kGolden = 0x9e3779b9;
#elif __SIZEOF_SIZE_T__ == 8
    constexpr size_t kGolden = 0x9e3779b97f4a7c17;
#endif
    std::size_t ret = 0;
    ret ^= std::hash<hook_func>()(pair.first) + kGolden + (ret << 6) + (ret >> 2);
    ret ^= std::hash<lib_base>()(pair.second) + kGolden + (ret << 6) + (ret >> 2);
    return ret;
  }
};

// (hook function, library of instruction which called hook function) -> original function
static std::unordered_map<std::pair<hook_func, lib_base>, prev_func, pairhash>& orig_functions() {
  static std::unordered_map<std::pair<hook_func, lib_base>, prev_func, pairhash> map_;
  return map_;
}
static pthread_rwlock_t orig_functions_mutex_ = PTHREAD_RWLOCK_INITIALIZER;

}

int
linker_initialize()
{
  if (sigmux_init(SIGSEGV) ||
      sigmux_init(SIGBUS))
  {
    return 1;
  }

  return refresh_shared_libs();
}

int
get_relocations(symbol sym, reloc* relocs_out, size_t relocs_out_len) {
  Dl_info info;
  if (!dladdr(sym, &info)) {
    errno = ENOENT;
    return -1;
  }

  try {
    auto lib = sharedLib(info.dli_sname);
    auto relocs = lib.get_relocations(sym);

    if (relocs.size() > relocs_out_len) {
      errno = ERANGE;
      return -1;
    }

    memcpy(relocs_out, relocs.data(), relocs.size() * sizeof(reloc));
    return relocs.size();
  } catch (std::out_of_range& e) {
    errno = ENODATA;
    return -1;
  }
}

int
patch_relocation_address(prev_func* plt_got_entry, hook_func hook) {
  Dl_info info;
  if (!hook || !dladdr(plt_got_entry, &info)) {
    return 1;
  }
  lib_base lib_being_hooked = info.dli_fbase;

  prev_func old_got_entry = *plt_got_entry;
  int rc = sig_safe_write(plt_got_entry, reinterpret_cast<intptr_t>(hook));

  // if we need to mprotect, it must be done under lock - don't want to set +w,
  // then have somebody else finish and set -w, before we're done with our write
  WriterLock wl(&orig_functions_mutex_);

  if (rc && errno == EFAULT) {
    int pagesize = getpagesize();
    void* page = PAGE_ALIGN(plt_got_entry, pagesize);

    if (mprotect(page, pagesize, PROT_READ | PROT_WRITE)) {
      return 1;
    }

    rc = sig_safe_write(plt_got_entry, reinterpret_cast<intptr_t>(hook));

    int old_errno = errno;
    if (mprotect(page, pagesize, PROT_READ)) {
      abort();
    };
    errno = old_errno;
  }

  if (rc == 0) {
    orig_functions()[std::make_pair(hook, lib_being_hooked)] = old_got_entry;

    /**
      * "Chaining" mechanism:
      * This library allows us to "link" hooks, that is,
      * func() -> hook_n() -> hook_n-1() ... -> hook_1() -> actual_func()
      * To account for the case where hook_n() lives in a library that has
      * not been hooked (and thus doesn't have an entry in the <orig_functions>
      * map) insert said entry here, so that the "chain" of hooks is never
      * broken.
      *
      * Think of the chain as a linked list, where lib_being_hooked's PLT always
      * points to HEAD, and each (func, calling_lib) entry in orig_functions must
      * point to the next function in the chain
      */
    auto oldHookEntry = orig_functions().find(std::make_pair(old_got_entry, lib_being_hooked));
    if (oldHookEntry != orig_functions().end()) {
      if (!dladdr(hook, &info)) {
        return 1; // wtf? maybe should abort, or perform this check earlier
      }
      lib_base lib_of_new_hook = info.dli_fbase;

      // 1. if libfoo has hooked write(2) calls from libart, then libart_write = libfoo_write_hook and
      //    orig_functions[libfoo_write_hook, libart] = write
      // 2. if libbar later hooks write(2) from libart, then libart_write = libbar_write_hook and
      //    orig_functions[libbar_write_hook, libart] = libfoo_write_hook
      // 3. however, notice: #2 will cause flow to go from libart_write -> libbar_write_hook -> libfoo_write_hook
      //    *BUT*, orig_functions[libfoo_write_hook, libbar] doesn't exist!
      // 4. So, we need to set up a chain and make orig_functions[libfoo_write_hook, libbar] = orig_functions[libfoo_write_hook, libart] (= write)
      // 5. libbaz then hooks write(2) in libart. libart_write = libbaz_write_hook, orig_functions[libbaz_write_hook, libart] = libbar_write_hook,
      //    orig_functions[libbar_write_hook, libbaz] = orig_functions[libbar_write_hook, libart] = libfoo_write_hook
      // ... etc etc etc ..
      auto existing_hook_record = std::make_pair(old_got_entry, lib_being_hooked);
      auto new_record_for_existing_hook = std::make_pair(old_got_entry, lib_of_new_hook);
      orig_functions()[new_record_for_existing_hook] = orig_functions()[existing_hook_record];
      orig_functions().erase(existing_hook_record);
    }
  }

  return rc;
}

int
hook_plt_method(const char* libname, const char* name, hook_func hook) {
  plt_hook_spec spec(libname, name, hook);
  if (hook_single_lib(libname, &spec, 1) == 0 && spec.hook_result == 1) {
    return 0;
  }
  return 1;
}

int hook_single_lib(char const* libname, plt_hook_spec* specs, size_t num_specs) {
  int failures = 0;

  try {
    auto elfData = sharedLib(libname);
    for (unsigned int specCnt = 0; specCnt < num_specs; ++specCnt) {
      plt_hook_spec& spec = specs[specCnt];

      ElfW(Sym) const* sym = elfData.find_symbol_by_name(spec.fn_name);
      if (!sym) {
        continue; // Did not find symbol in the hash table, so go to next spec
      }

      for (prev_func* plt_got_entry : elfData.get_plt_relocations(sym)) {
        if (patch_relocation_address(plt_got_entry, spec.hook_fn) == 0) {
          spec.hook_result++;
        } else {
          failures++;
        }
      }
    }
  } catch (std::out_of_range& e) { /* no op */ }

  return failures;
}

int
hook_all_libs(plt_hook_spec* specs, size_t num_specs,
    bool (*allowHookingLib)(char const* libname, void* data), void* data) {
  if (refresh_shared_libs()) {
    // Could not properly refresh the cache of shared library data
    return -1;
  }

  int failures = 0;

  for (auto const& lib : allSharedLibs()) {
    if (allowHookingLib(lib.first.c_str(), data)) {
      failures += hook_single_lib(lib.first.c_str(), specs, num_specs);
    }
  }

  return failures;
}

static lib_base return_addr_base(void* return_address) {
  // write-through dladdr(2) cache, basically

  // note: if you're like @csarbora, you may spend twenty minutes or more thinking
  // about why we need a second map at all, and whether or not this cache functionality
  // couldn't just be integrated directly into the orig_functions map.
  //
  // to save you the time, we do NOT want to integrate this into the orig_functions map
  // because when someone re-hooks a hooked function, we need to be able to fix up
  // that original hook record. (the original hook record knows what its CALL_PREV should
  // be when it gets called from the library being hooked, but it doesn't know what its
  // CALL_PREV should be when it gets called from the library containing the new, second,
  // hook.) if we looked up by insn site instead of just lib_base in lookup_prev_plt_method,
  // then we'd need to fix up ALL the entries for a hooked function (lib base + all the insn
  // sites), which we can't look up efficiently.

  static std::unordered_map<void*, lib_base> cache_;
  static pthread_rwlock_t cacheMutex_ = PTHREAD_RWLOCK_INITIALIZER;

  {
    ReaderLock rl(&cacheMutex_);
    auto it = cache_.find(return_address);
    if (it != cache_.end()) {
      return it->second; // cache hit!
    }
  }

  Dl_info info;
  if (!dladdr(return_address, &info)) {
    // only way i can think of this ever happening is if a CALL_PREV resolved to a function that
    // got unloaded, which could only happen if it were something that was dlopen'ed (or was a
    // dominated dep of something that was dlopen'ed) and that something was then later dlclose'ed
    // *while* the call stack was still executing. this race is inherent to dlclose() however; it's
    // still possible that a thread executing foo() in libbar blows up because libbar unloads
    std::stringstream ss;
    ss << "return_address " << std::hex << return_address << " not part of our process";
    log_assert(ss.str().c_str());
  }

  // we can release the ReaderLock and later grab the WriterLock - without worrying about
  // the state of the set changing from under us - because in a race we're simply re-adding
  // an identical pair. return_address is not going to map to more than one lib_base unless
  // dlclose starts racing with us, per ^ comment, so this emplace call won't have diff data
  WriterLock wl(&cacheMutex_);
  cache_.emplace(return_address, info.dli_fbase);
  return info.dli_fbase;
}

prev_func lookup_prev_plt_method(
    void* hook_insn,
    void* return_address,
    char const* file,
    int line) {
  lib_base calling_lib = return_addr_base(return_address);

  ReaderLock rl(&orig_functions_mutex_);

  auto method_it = orig_functions().find(std::make_pair(hook_insn, calling_lib));
  if (method_it == orig_functions().cend()) {
    std::stringstream ss;
    ss << file << ":" << line << " unable to find previous method";
    log_assert(ss.str().c_str());
  }
  return method_it->second;
}
