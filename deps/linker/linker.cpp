// Copyright 2004-present Facebook. All Rights Reserved.

/* -std=c++14 triggers __STRICT_ANSI__, which stops stdbool.h defining this */
#define _Bool bool
#include "bionic_linker.h"
#include "linker.h"

#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <sig_safe_write/sig_safe_write.h>
#include <cjni/log.h>
#include <sigmux.h>

#include <map>
#include <unordered_map>

#define PAGE_ALIGN(ptr, pagesize) (void*) (((uintptr_t) (ptr)) & ~((pagesize) - 1))

namespace {

struct fault_handler_data {
  void* address;
  int tid;
  int active;
  jmp_buf jump_buffer;
};

// hook -> lib -> orig_func
static std::map<void*, std::unordered_map<void*, void*>>* orig_functions;
static pthread_rwlock_t plt_mutex = PTHREAD_RWLOCK_INITIALIZER;

// ndk r10e and c++14 don't play nice. until fblite moves to r12, do our own
class ReaderLock {
private:
  pthread_rwlock_t *_lock;

public:
  inline ReaderLock(pthread_rwlock_t *lock) :
    _lock(lock) {
    if (pthread_rwlock_rdlock(lock) != 0) {
      abort();
    }
  }

  inline ~ReaderLock() {
    if (pthread_rwlock_unlock(_lock) != 0) {
      abort();
    }
  }
};

class WriterLock {
private:
  pthread_rwlock_t *_lock;

public:
  inline WriterLock(pthread_rwlock_t *lock) :
    _lock(lock) {
    if (pthread_rwlock_wrlock(lock) != 0) {
      abort();
    }
  }

  inline ~WriterLock() {
    if (pthread_rwlock_unlock(_lock) != 0) {
      abort();
    }
  }
};

static int
get_sdk_version()
{
  char data[PROP_VALUE_MAX];
  memset(data, 0, PROP_VALUE_MAX);

  if (!__system_property_get("ro.build.version.sdk", data)) {
    return 0;
  }
  return atoi(data);
}

static unsigned elfhash(const char* _name) {
  const unsigned char* name = reinterpret_cast<const unsigned char*>(_name);
  unsigned h = 0, g;

  while (*name) {
    h = (h << 4) + *name++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

/**
 * Finds a GOT entry which is used by PLT associated with given name in given
 * library.
 * On failure returns nullptr and errno is set.
 */
static void*
find_got_entry_for_plt_function(
  void* dlhandle,
  const char* name
) {
  struct soinfo* si = (struct soinfo*) dlhandle;

  if (!si) {
    errno = EINVAL;
    return nullptr;
  }

  // Alternative way to check if a symbol is defined in the given library is
  // to use symbol hash table (HASH section) and fail fast avoiding iteration
  // over plt records.
  // NOTE: This won't work for GNU_HASH section which is supported for API >= 23
  bool use_hash = si->nbucket_ != 0;
  uint32_t sym_index = 0;

  if (use_hash) {
    auto hash = elfhash(name);
    sym_index = si->bucket_[hash % si->nbucket_];
    for (; sym_index != 0; sym_index = si->chain_[sym_index]) {
      Elf32_Sym symbol = si->symtab_[sym_index];
      if (strcmp(si->strtab_ + symbol.st_name, name) == 0) {
        break;
      }
    }
    if (sym_index == 0) {
      errno = EINVAL;
      return nullptr;
    }
  }

  for (unsigned i = 0; i < si->plt_rel_count_; ++i) {
    Elf32_Rel rel = si->plt_rel_[i];
    unsigned rel_type = ELF32_R_TYPE(rel.r_info);
    unsigned rel_sym = ELF32_R_SYM(rel.r_info);

    if (rel_type != PLT_RELOCATION_TYPE || rel_sym == 0) {
      continue;
    }

    bool plt_found = false;
    if (use_hash) {
      plt_found = rel_sym == sym_index;
    } else {
      Elf32_Sym symbol = si->symtab_[rel_sym];
      const char* symbol_name = si->strtab_ + symbol.st_name;
      plt_found = strcmp(symbol_name, name) == 0;
    }

    if (plt_found) {
      if (get_sdk_version() >= 17) {
        return (void*) (rel.r_offset + si->load_bias);
      } else { // the linker did not support load_bias before 17
        return (void*) (rel.r_offset + si->base);
      }
    }
  }

  errno = ENOENT;
  return nullptr;
}

} // namespace (anonymous)

int
linker_initialize()
{
  // We can't perform any of these tricks on N or above - dlopen(3) returns a
  // handle and not a bare pointer starting with N.
  bool is_n_or_above = get_sdk_version() >= 24;

  if(is_n_or_above) {
    errno = ENOTSUP;
    return 1;
  }

  if (sigmux_init(SIGSEGV) ||
      sigmux_init(SIGBUS))
  {
    return 1;
  }

  WriterLock wl(&plt_mutex);
  if (orig_functions == nullptr) {
    orig_functions = new(std::nothrow) std::map<void*, std::unordered_map<void*, void*>>();
    if (orig_functions == nullptr) {
      errno = ENOMEM;
      return 1;
    }
  }

  return 0;
}

int
hook_plt_method_impl(void* dlhandle, const char* name, void* hook)
{
  if (orig_functions == nullptr) {
    errno = EPERM;
    return 1;
  }

  void* plt_got_entry = find_got_entry_for_plt_function(dlhandle, name);
  if (plt_got_entry == nullptr) {
    return 1;
  }

  Dl_info info;
  if (!dladdr(plt_got_entry, &info)) {
    return 1;
  }

  void* old_got_entry = *(void**)plt_got_entry;
  int rc = sig_safe_write(plt_got_entry, (intptr_t)hook);

  if (rc && errno == EFAULT) {
    int pagesize = getpagesize();
    void* page = PAGE_ALIGN(plt_got_entry, pagesize);

    if (mprotect(page, pagesize, PROT_READ | PROT_WRITE)) {
      return 1;
    }

    rc = sig_safe_write(plt_got_entry, (intptr_t)hook);

    int old_errno = errno;
    if (mprotect(page, pagesize, PROT_READ)) {
      abort();
    };
    errno = old_errno;
  }

  if (!rc) {
    (*orig_functions)[hook][info.dli_fbase] = old_got_entry;
  }

  return rc;
}

int
hook_plt_method(void* dlhandle, const char* name, void* hook) {
  WriterLock wl(&plt_mutex);
  return hook_plt_method_impl(dlhandle, name, hook);
}

void
hook_plt_methods(plt_hook_spec* hook_specs, size_t num_hook_specs) {
  WriterLock wl(&plt_mutex);

  for (size_t i=0; i < num_hook_specs; ++i) {
    plt_hook_spec& spec = hook_specs[i];
    void* handle = dlopen(spec.lib_name, RTLD_LOCAL);
    if (handle == nullptr) {
      spec.dlopen_result = 1;
      continue;
    }

    spec.dlopen_result = 0;
    spec.hook_result = hook_plt_method_impl(handle, spec.fn_name, spec.hook_fn);
    dlclose(handle);
  }
}

void*
lookup_prev_plt_method(void* hook, void* return_address) {
  if (orig_functions == nullptr) {
    return nullptr;
  }

  Dl_info info;
  if (!dladdr(return_address, &info)) {
    return nullptr;
  }

  ReaderLock rl(&plt_mutex);

  std::map<void*, std::unordered_map<void*, void*>>::const_iterator hook_iter =
    orig_functions->upper_bound(hook);
  hook_iter--;

  auto& method_map = (*hook_iter).second;
  auto method_it = method_map.find(info.dli_fbase);
  if (method_it != method_map.cend()) {
    return (*method_it).second;
  }
  return nullptr;
}

__attribute__((noinline))
void*
get_pc_thunk() {
  return __builtin_return_address(0);
}
