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

#include <plthooks/hooks.h>
#include <linker/locks.h>
#include <linker/log_assert.h>
#include <plthooks/trampoline.h>
#include <plthooks/plthooks.h>

#include <errno.h>
#include <sys/mman.h>

#include <abort_with_reason.h>
#include <cjni/log.h>
#include <linker/linker.h>
#include <linker/sharedlibs.h>
#include <sig_safe_write/sig_safe_write.h>
#include <sigmux.h>

#define PAGE_ALIGN(ptr, pagesize) (void*) (((uintptr_t) (ptr)) & ~((pagesize) - 1))

using namespace facebook::linker;
using namespace facebook::plthooks;

typedef void* prev_func;
typedef void* lib_base;

namespace {

// Global lock on any GOT slot modification.
static pthread_rwlock_t g_got_modification_lock = PTHREAD_RWLOCK_INITIALIZER;

}

int
plthooks_initialize() {
  if (linker_initialize()) {
    return 1;
  }

  if (sigmux_init(SIGSEGV) ||
      sigmux_init(SIGBUS))
  {
    return 1;
  }

  return 0;
}

int unsafe_patch_relocation_address(
    prev_func* plt_got_entry,
    hook_func new_value) {
  try {
    int rc =
        sig_safe_write(plt_got_entry, reinterpret_cast<intptr_t>(new_value));

    if (rc && errno == EFAULT) {
      // if we need to mprotect, it must be done under lock - don't want to
      // set +w, then have somebody else finish and set -w, before we're done
      // with our write
      static pthread_rwlock_t mprotect_mutex_ = PTHREAD_RWLOCK_INITIALIZER;
      WriterLock wl(&mprotect_mutex_);

      int pagesize = getpagesize();
      void* page = PAGE_ALIGN(plt_got_entry, pagesize);

      if (mprotect(page, pagesize, PROT_READ | PROT_WRITE)) {
        return 5;
      }

      rc = sig_safe_write(plt_got_entry, reinterpret_cast<intptr_t>(new_value));

      int old_errno = errno;
      if (mprotect(page, pagesize, PROT_READ)) {
        abort();
      };
      errno = old_errno;
    }

    return rc;
  } catch (std::runtime_error const&) {
    return 6;
  }
}

int
patch_relocation_address_for_hook(prev_func* plt_got_entry, plt_hook_spec* spec) {
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
      .new_function = spec->hook_fn,
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
  hooks::HookInfo hook_info {
    .out_id = 0,
    .got_address = got_addr,
    .new_function = spec->hook_fn,
    .previous_function = *plt_got_entry,
  };
  auto ret = hooks::add(hook_info);
  if (ret != hooks::NEW_HOOK) {
    return 1;
  }
  hook_func trampoline = create_trampoline(hook_info.out_id);

  return unsafe_patch_relocation_address(plt_got_entry, trampoline);
}

static bool
verify_got_entry_for_spec(prev_func* got_addr, plt_hook_spec* spec) {
  if (hooks::is_hooked(reinterpret_cast<uintptr_t>(got_addr))) {
    // We've done this already, stop checking.
    return true;
  }

  Dl_info info;
  if (!dladdr(got_addr, &info)) {
    LOGE("GOT entry not part of a DSO: %p", got_addr);
    return false;
  }
  if (!dladdr(*got_addr, &info)) {
    LOGE("GOT entry does not point to a DSO: %p", *got_addr);
    return false;
  }
  if (info.dli_sname == nullptr || strcmp(info.dli_sname, spec->fn_name)) {
    LOGE(
        "GOT entry does not point to symbol we need: %s vs %s",
        info.dli_sname,
        spec->fn_name);
    return false;
  }
  return true;
}

int hook_plt_method(const char* libname, const char* name, hook_func hook) {
  plt_hook_spec spec(name, hook);
  auto ret = hook_single_lib(libname, &spec, 1);
  if (ret == 0 && spec.hook_result == 1) {
    return 0;
  }
  return 1;
}

int unhook_plt_method(const char* libname, const char* name, hook_func hook) {
  plt_hook_spec spec(name, hook);
  if (unhook_single_lib(libname, &spec, 1) == 0 && spec.hook_result == 1) {
    return 0;
  }
  return 1;
}

int hook_single_lib(char const* libname, plt_hook_spec* specs, size_t num_specs) {
  int failures = 0;

  try {
    auto elfData = sharedLib(libname);
    for (unsigned int specCnt = 0; specCnt < num_specs; ++specCnt) {
      plt_hook_spec* spec = &specs[specCnt];

      if (!spec->hook_fn || !spec->fn_name) {
        // Invalid spec.
        failures++;
        continue;
      }

      ElfW(Sym) const* sym = elfData.find_symbol_by_name(spec->fn_name);
      if (!sym) {
        continue; // Did not find symbol in the hash table, so go to next spec
      }

      for (prev_func* plt_got_entry : elfData.get_plt_relocations(sym)) {

        // Run sanity checks on what we parsed as the GOT slot.
        if (!verify_got_entry_for_spec(plt_got_entry, spec)) {
           failures++;
           continue;
        }

        if (patch_relocation_address_for_hook(plt_got_entry, spec) == 0) {
          spec->hook_result++;
        } else {
          failures++;
        }
      }
    }
  } catch (std::out_of_range& e) { /* no op */ }

  return failures;
}

int unhook_single_lib(
    char const* libname,
    plt_hook_spec* specs,
    size_t num_specs) {
  int failures = 0;

  try {
    auto elfData = sharedLib(libname);

    // Take the GOT lock to prevent other threads from modifying our state.
    WriterLock lock(&g_got_modification_lock);

    for (unsigned int specCnt = 0; specCnt < num_specs; ++specCnt) {
      plt_hook_spec& spec = specs[specCnt];

      ElfW(Sym) const* sym = elfData.find_symbol_by_name(spec.fn_name);
      if (!sym) {
        continue; // Did not find symbol in the hash table, so go to next spec
      }
      for (prev_func* plt_got_entry : elfData.get_plt_relocations(sym)) {
        auto addr = reinterpret_cast<uintptr_t>(plt_got_entry);
        // Remove the entry for this GOT address and this particular hook.
        hooks::HookInfo info{
            .got_address = addr,
            .new_function = spec.hook_fn,
        };
        auto result = hooks::remove(info);
        if (result == hooks::REMOVED_STILL_HOOKED) {
          // There are other hooks at this slot, continue.
          spec.hook_result++;
          continue;
        } else if (result == hooks::REMOVED_TRIVIAL) {
          // Only one entry left at this slot, patch the original function
          // to lower the overhead.
          auto original = info.previous_function;
          if (unsafe_patch_relocation_address(plt_got_entry, original) != 0) {
            abortWithReason("Unable to unhook GOT slot");
          }
          // Restored the GOT slot, let's remove all knowledge about this
          // hook.
          hooks::HookInfo original_info{
              .got_address = reinterpret_cast<uintptr_t>(plt_got_entry),
              .new_function = original,
          };
          if (hooks::remove(original_info) != hooks::REMOVED_FULLY) {
            abortWithReason("GOT slot modified while we were working on it");
          }
          spec.hook_result++;
          continue;
        } else if (result == hooks::UNKNOWN_HOOK) {
          // Either unhooked or hooked but not with this hook.
          continue;
        } else {
          failures++;
        }
      }
    }
  } catch (std::out_of_range& e) { /* no op */
  }

  return failures;
}

int hook_all_libs(
    plt_hook_spec* specs,
    size_t num_specs,
    AllowHookingLibCallback allowHookingLib,
    void* data) {
  if (refresh_shared_libs()) {
    // Could not properly refresh the cache of shared library data
    return -1;
  }

  int failures = 0;

  for (auto const& lib : allSharedLibs()) {
    if (allowHookingLib(lib.first.c_str(), lib.second.getLibName(), data)) {
      failures += hook_single_lib(lib.first.c_str(), specs, num_specs);
    }
  }

  return failures;
}

int unhook_all_libs(
    plt_hook_spec* specs,
    size_t num_specs) {
  int failures = 0;

  for (auto const& lib : allSharedLibs()) {
    failures += unhook_single_lib(lib.first.c_str(), specs, num_specs);
  }

  return failures;
}
