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

/* -std=c++14 triggers __STRICT_ANSI__, which stops stdbool.h defining this */
#define _Bool bool
#include "bionic_linker.h"
#include "linker.h"
#include "elf_pltrel_parser.h"

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
#include <string>

#include <sig_safe_write/sig_safe_write.h>
#include <cjni/log.h>
#include <sigmux.h>

#include <cinttypes>
#include <map>
#include <unordered_map>
#include <algorithm>

#define PAGE_ALIGN(ptr, pagesize) (void*) (((uintptr_t) (ptr)) & ~((pagesize) - 1))

#define ANDROID_L  21

#if defined(__x86__64__) || defined(__aarch64__)
# define ELF_RELOC_TYPE ELF64_R_TYPE
# define ELF_RELOC_SYM  ELF64_R_SYM
#else
# define ELF_RELOC_TYPE ELF32_R_TYPE
# define ELF_RELOC_SYM  ELF32_R_SYM
#endif

namespace {

struct fault_handler_data {
  void* address;
  int tid;
  int active;
  jmp_buf jump_buffer;
};

typedef struct _elfSharedLibData {
  uint8_t* loadBias;
  ElfW(Rel) const* pltRelTable;
  size_t pltRelTableLen;
  ElfW(Sym) const* dynSymbolsTable;
  char const* dynStrsTable;

  // Fields used only for Android < 21 (L)
  uint32_t* hashBucket;
  size_t nHashBucket;
  uint32_t* hashChain;

  _elfSharedLibData() :
    loadBias(nullptr),
    pltRelTable(nullptr),
    pltRelTableLen(0),
    dynSymbolsTable(nullptr),
    dynStrsTable(nullptr),
    hashBucket(nullptr),
    nHashBucket(0),
    hashChain(nullptr)
  {}

} elfSharedLibData;

static std::unordered_map<std::string, elfSharedLibData> sharedLibData;

// hook -> lib -> orig_func
static std::map<void*, std::unordered_map<void*, void*>>* orig_functions;
static pthread_rwlock_t plt_mutex = PTHREAD_RWLOCK_INITIALIZER;

// ndk r10e and c++14 don't play nice. until fblite moves to r12, do our own
class ReaderLock {
private:
  pthread_rwlock_t *_lock;
  bool _disabled;

public:
  inline ReaderLock(pthread_rwlock_t *lock) :
    _lock(lock),
    _disabled(false) {
    auto ret = pthread_rwlock_rdlock(lock); 
    if (ret == EDEADLK) {
      _disabled = true;
    } else if (ret != 0) {
      abort();
    }
  }

  inline ~ReaderLock() {
    if (_disabled) {
      return;
    }
    auto ret = pthread_rwlock_unlock(_lock);
    if (ret != 0) {
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

bool
ends_with(const char* str, const char* ending) {
  size_t str_len = strlen(str);
  size_t ending_len = strlen(ending);

  if (ending_len > str_len) {
    return false;
  }
  return strcmp(str + (str_len - ending_len), ending) == 0;
}

} // namespace (anonymous)

typedef int (*dl_iterate_phdr_type)(int (*)(struct dl_phdr_info* info,
      size_t size, void* data), void* data);

// Parse all the shared libraries from the executable, storing the necessary
// information to find the relocation symbols and addresses we want to patch.
static int dl_iterate_phdr_cb(struct dl_phdr_info* info, size_t size, void* data) {
  if (!info->dlpi_name || info->dlpi_name[0] == '\0') {
    // Go to the next shared library
    return 0;
  }

  if (!ends_with(info->dlpi_name, ".so")) {
    return 0;
  }

  if (sharedLibData.find(info->dlpi_name) != sharedLibData.cend()) {
    // Library already parsed, go to the next one.
    return 0;
  }

  elfSharedLibData elfData {};
  ElfW(Dyn) const* dynamic_table = nullptr;

  elfData.loadBias = (uint8_t*)info->dlpi_addr;

  for (int i = 0; i < info->dlpi_phnum; ++i) {
    ElfW(Phdr) const* phdr = &info->dlpi_phdr[i];
    if (phdr->p_type == PT_DYNAMIC) {
      dynamic_table = reinterpret_cast<ElfW(Dyn) const*>(elfData.loadBias + phdr->p_vaddr);
      break;
    }
  }

  if (dynamic_table == nullptr) {
    // Failed to find the necessary segments. Go to next library.
    return 0;
  }

  bool pltRelTableLenFound = false;
  bool pltRelTableFound = false;
  bool dynSymbolsTableFound = false;
  bool dynStrsTableFound = false;

  for (ElfW(Dyn) const* entry = dynamic_table; entry && entry->d_tag != DT_NULL; ++entry) {
    switch (entry->d_tag) {
      case DT_PLTRELSZ:
        elfData.pltRelTableLen = entry->d_un.d_val / sizeof(ElfW(Rel));
        pltRelTableLenFound = true;
        break;

      case DT_JMPREL:
        elfData.pltRelTable =
          reinterpret_cast<ElfW(Rel) const*>(elfData.loadBias + entry->d_un.d_ptr);
        pltRelTableFound = true;
        break;

      case DT_SYMTAB:
        elfData.dynSymbolsTable =
          reinterpret_cast<ElfW(Sym) const*>(elfData.loadBias + entry->d_un.d_ptr);
        dynSymbolsTableFound = true;
        break;

      case DT_STRTAB:
        elfData.dynStrsTable =
          reinterpret_cast<char const*>(elfData.loadBias + entry->d_un.d_ptr);
        dynStrsTableFound = true;
        break;
    }

    if (pltRelTableLenFound && pltRelTableFound &&
        dynSymbolsTableFound && dynStrsTableFound) {
      break;
    }
  }

  if (!pltRelTableLenFound || !pltRelTableFound ||
      !dynSymbolsTableFound || !dynStrsTableFound) {
    // Error, go to next library
    return 0;
  }

  sharedLibData.emplace(info->dlpi_name, std::move(elfData));
  return 0;
}

int
refresh_shared_libs() {

  int const sdk_version = get_sdk_version();

  if (sdk_version >= ANDROID_L) {
    dl_iterate_phdr_type phdr_iter_function =
      (dl_iterate_phdr_type) dlsym(RTLD_DEFAULT, "dl_iterate_phdr");

    if (phdr_iter_function == nullptr) {
      // Undefined symbol.
      return 1;
    }

    (*phdr_iter_function)(dl_iterate_phdr_cb, nullptr);
  } else {
    soinfo* si = reinterpret_cast<soinfo*>(dlopen(nullptr, RTLD_LOCAL));

    if (si == nullptr) {
      return 1;
    }

    for (; si != nullptr; si = si->next) {
      if (!si->link_map_head.l_name) {
        continue;
      }

      char const* libname = si->link_map_head.l_name;
      char const* const so_ext = ".so";

      if (!ends_with(libname, so_ext)) {
        continue;
      }

      if (sharedLibData.find(libname) != sharedLibData.cend()) {
        // Library already parsed, go to the next one.
        return 0;
      }

      elfSharedLibData elfData {};

      elfData.pltRelTableLen = si->plt_rel_count_;
      elfData.pltRelTable = si->plt_rel_;
      elfData.dynSymbolsTable = si->symtab_;
      elfData.dynStrsTable = si->strtab_;
      elfData.hashBucket = si->bucket_;
      elfData.nHashBucket = si->nbucket_;
      elfData.hashChain = si->chain_;


      if (sdk_version >= 17) {
        elfData.loadBias = reinterpret_cast<uint8_t*>(si->load_bias);
      } else {
        elfData.loadBias = reinterpret_cast<uint8_t*>(si->base);
      }

      sharedLibData.emplace(libname, std::move(elfData));
    }
  }
  return 0;
}

int
linker_initialize()
{
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

  return refresh_shared_libs();
}

int
patch_relocation_address(void* plt_got_entry, void* hook) {
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

    /**
      * "Chaining" mechanism:
      * This library allows us to "link" hooks, that is,
      * func() -> hook_n() -> hook_n-1() ... -> hook_1() -> actual_func()
      * To account for the case where hook_n() lives in a library that has
      * not been hooked (and thus doesn't have an entry in the <orig_functions>
      * map) insert said entry here, so that the "chain" of hooks is never
      * broken.
      */
    auto oldHookEntry = orig_functions->find(old_got_entry);
    if (oldHookEntry != orig_functions->end()) {
      auto oldLibEntry = oldHookEntry->second.find(info.dli_fbase);
      if (oldLibEntry != oldHookEntry->second.end()) {
        // <oldHookEntry, oldLibEntry> already exists. Insert an entry for
        // <oldHookEntry, newLib> into our map.
        Dl_info info2;
        if (!dladdr(hook, &info2)) {
          return 1;
        }
        (*orig_functions)[old_got_entry][info2.dli_fbase] =
            (*orig_functions)[old_got_entry][info.dli_fbase];
      }
    }
  }

  return rc;
}

int
hook_plt_method(void* dlhandle, const char* libname, const char* name, void* hook) {

  plt_hook_spec spec(nullptr, name, hook);
  return hook_single_lib(dlhandle, libname, &spec, 1);
}

/* It can happen that, after caching a shared object's data in sharedLibData,
 * the library is unloaded, so references to memory in that address space
 * result in SIGSEGVs. Thus, check here that the addresses are still valid.
*/
bool
validateSharedLibData(elfSharedLibData const* elfData) {
  Dl_info info;
  if (!dladdr(elfData->pltRelTable, &info)) {
    return false;
  }

  if (!dladdr(elfData->dynSymbolsTable, &info)) {
    return false;
  }

  if (!dladdr(elfData->dynStrsTable, &info)) {
    return false;
  }

  return true;
}

int hook_single_lib(
    void* dlhandle,
    char const* libname,
    plt_hook_spec* specs,
    size_t num_specs) {

  if (orig_functions == nullptr) {
    errno = EPERM;
    return 1;
  }

  WriterLock wl(&plt_mutex);

  elfSharedLibData const* elfData;
  try {
    elfData = &sharedLibData.at(libname);
  } catch (std::out_of_range& e) {
    // We didn't cache this library during dl_iterate_phdr
    return 1;
  }

  // Make sure every piece of data we need is valid (i.e. lives within
  // a shared object we actually loaded).
  if (!validateSharedLibData(elfData)) {
    return 1;
  }

  bool const use_hash = elfData->nHashBucket > 0;
  bool hooking_just_one = num_specs == 1;
  bool hooked_some = false;

  for (unsigned int specCnt = 0; specCnt < num_specs; ++specCnt) {
    const plt_hook_spec& spec = specs[specCnt];
    char const* functionName = spec.fn_name;
    void* hook = spec.hook_fn;
    uint32_t sym_index = 0;

    // Fail-fast method. Will work only for Android < L
    if (use_hash) {
      auto hash = elfhash(functionName);
      sym_index = elfData->hashBucket[hash % elfData->nHashBucket];
      for (; sym_index != 0; sym_index = elfData->hashChain[sym_index]) {
        ElfW(Sym) const& symbol = elfData->dynSymbolsTable[sym_index];
        if (strcmp(elfData->dynStrsTable + symbol.st_name, functionName) == 0) {
          break;
        }
      }
      if (sym_index == 0) {
        // Did not find symbol in the hash table, so go to next spec
        continue;
      }
    }

    for (unsigned int i = 0; i < elfData->pltRelTableLen; i++) {
      ElfW(Rel) const& rel = elfData->pltRelTable[i];
      unsigned rel_type = ELF_RELOC_TYPE(rel.r_info);
      unsigned rel_sym = ELF_RELOC_SYM(rel.r_info);

      if (rel_type != PLT_RELOCATION_TYPE || !rel_sym) {
        continue;
      }

      bool plt_found = false;

      if (use_hash) {
        plt_found = rel_sym == sym_index;
      } else {
        ElfW(Sym) const& symbol = elfData->dynSymbolsTable[rel_sym];
        char const* symbol_name = elfData->dynStrsTable + symbol.st_name;
        plt_found = strcmp(symbol_name, functionName) == 0;
      }

      if (plt_found) {
        // Found the address of the relocation
        void* plt_got_entry = elfData->loadBias + rel.r_offset;
        patch_relocation_address(plt_got_entry, hook);
        hooked_some = true;
        break;
      }
    }
  }

  if (hooking_just_one && !hooked_some) {
    // Bad: we attempted hooking a single library and were unable to.
    return 1;
  }

  return 0;
}

int
hook_all_libs(plt_hook_spec* specs, size_t num_specs,
    bool (*allowHookingLib)(char const* libname, void* data), void* data) {

  char const* libname;

  int refreshRet = refresh_shared_libs();

  if (refreshRet) {
    // Could not properly refresh the cache of shared library data
    return 1;
  }

  for (auto const& elfMap : sharedLibData) {
    libname = elfMap.first.c_str();

    if (!allowHookingLib(libname, data)) {
      continue;
    }

    // Hook every <spec.fn_name> with <spec.hook_fn> in <libname>
    hook_single_lib(nullptr, libname, specs, num_specs);
  }

  return 0;
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
