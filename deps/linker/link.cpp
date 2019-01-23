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
#include <linker/sharedlibs.h>
#include <linker/log_assert.h>

#include <errno.h>
#include <stdexcept>
#include <libgen.h>

using namespace facebook::linker;

#ifdef __ANDROID__

extern "C" {

int dladdr1(void* addr, Dl_info* info, void** extra_info, int flags) {
  if (flags != RTLD_DL_SYMENT) {
    errno = ENOSYS;
    return 0;
  }

  if (!dladdr(addr, info)) {
    return 0; // docs specify dlerror not set in this case, which makes it easy for us
  }

  elfSharedLibData lib;
  try {
    lib = sharedLib(basename(info->dli_fname));
  } catch (std::out_of_range& e) {
    return 0;
  }

  auto sym_info = const_cast<ElfW(Sym) const**>(reinterpret_cast<ElfW(Sym)**>(extra_info));
  *sym_info = lib.find_symbol_by_name(info->dli_sname);
  if (*sym_info) {
    if (lib.getLoadedAddress(*sym_info) != info->dli_saddr) {
      log_assert(
        "tried to resolve address 0x%x but dladdr returned \"%s\" (0x%x) while find_symbol_by_name returned %x",
        addr,
        info->dli_sname,
        info->dli_saddr,
        (*sym_info)->st_value);
    }
    return 1;
  }

  return 0;
}

} // extern "C"

#endif // __ANDROID__
