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

#include <linker/linker.h>
#include <linker/sharedlibs.h>

#include <dlfcn.h>
#include <errno.h>

#include <atomic>

#define PAGE_ALIGN(ptr, pagesize) (void*) (((uintptr_t) (ptr)) & ~((pagesize) - 1))

using namespace facebook::linker;

typedef void* prev_func;
typedef void* lib_base;

namespace {

static std::atomic<bool> g_linker_enabled(true);

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
