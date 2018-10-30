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
#include <linker/hooks.h>
#include <linker/locks.h>
#include <linker/log_assert.h>
#include <linker/trampoline.h>

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

#include <atomic>
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

static std::atomic<bool> g_linker_enabled(true);

// Global lock on any GOT slot modification.
static pthread_rwlock_t g_got_modification_lock = PTHREAD_RWLOCK_INITIALIZER;

}

void
linker_set_enabled(int enabled)
{
  g_linker_enabled = enabled;
}

int
linker_initialize()
{
  if (!g_linker_enabled.load()) {
    return 1;
  }
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
  // We've never hooked this GOT slot before.
  Dl_info info;
  if (!hook || !dladdr(plt_got_entry, &info)) {
    return 1;
  }

  auto got_addr = reinterpret_cast<uintptr_t>(plt_got_entry);

  // Take the pessimistic writer lock. This enforces a global serial order
  // on GOT slot modifications but makes the code much easier to reason about.
  // For slots that we've already hooked, this is overkill but is easier
  // than tracking modification conflicts.

  WriterLock lock(&g_got_modification_lock);
  if (hooks::is_hooked(got_addr)) {
    // No point in safety checks if we've already once hooked this GOT slot.
    hooks::HookInfo info {
      .out_id = 0,
      .got_address = got_addr,
      .new_function = hook,
      .previous_function = *plt_got_entry,
    };
    auto ret = hooks::add(info);
    if (ret != hooks::ALREADY_HOOKED_APPENDED) {
      return 1;
    } else {
      return 0;
    }
  }

  // We haven't hooked this slot yet. We also need to make a trampoline.
  try {
    hooks::HookInfo info {
      .out_id = 0,
      .got_address = got_addr,
      .new_function = hook,
      .previous_function = *plt_got_entry,
    };
    auto ret = hooks::add(info);
    if (ret != hooks::NEW_HOOK) {
      return 1;
    }
    hook_func trampoline = create_trampoline(info.out_id);

    int rc = sig_safe_write(plt_got_entry, reinterpret_cast<intptr_t>(trampoline));

    if (rc && errno == EFAULT) {
      // if we need to mprotect, it must be done under lock - don't want to set +w,
      // then have somebody else finish and set -w, before we're done with our write
      static pthread_rwlock_t mprotect_mutex_ = PTHREAD_RWLOCK_INITIALIZER;
      WriterLock wl(&mprotect_mutex_);

      int pagesize = getpagesize();
      void* page = PAGE_ALIGN(plt_got_entry, pagesize);

      if (mprotect(page, pagesize, PROT_READ | PROT_WRITE)) {
        return 2;
      }

      rc = sig_safe_write(plt_got_entry, reinterpret_cast<intptr_t>(trampoline));

      int old_errno = errno;
      if (mprotect(page, pagesize, PROT_READ)) {
        abort();
      };
      errno = old_errno;
    }

    return rc;
  } catch (std::runtime_error const&) {
    return 3;
  }
}

int
hook_plt_method(const char* libname, const char* name, hook_func hook) {
  plt_hook_spec spec(name, hook);
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
