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

#include <linker/sharedlibs.h>
#include <linker/locks.h>

#include <build/build.h>

#include <stdlib.h>
#include <dlfcn.h>
#include <unordered_map>
#include <libgen.h>
#include <link.h>
#include "bionic_linker.h"

#ifndef __ANDROID__
char * basename(char const* path);
#endif

#define DT_GNU_HASH	0x6ffffef5	/* GNU-style hash table.  */

namespace facebook {
namespace linker {

namespace {

static pthread_rwlock_t sharedLibsMutex_ = PTHREAD_RWLOCK_INITIALIZER;
static std::unordered_map<std::string, elfSharedLibData>& sharedLibData() {
  static std::unordered_map<std::string, elfSharedLibData> sharedLibData_;
  return sharedLibData_;
}

bool addSharedLib(ElfW(Addr) addr, const char* name, const ElfW(Phdr)* phdr, ElfW(Half) phnum) {
  auto libbasename = basename(name);
  {
    ReaderLock rl(&sharedLibsMutex_);
    // the common path will be duplicate entries, so skip the weight of
    // elfSharedLibData construction and grabbing a writer lock, if possible
    if (sharedLibData().find(libbasename) != sharedLibData().end()) {
      return false;
    }
  }

  try {
    elfSharedLibData data(addr, name, phdr, phnum);
    WriterLock wl(&sharedLibsMutex_);
    return sharedLibData().insert(std::make_pair(libbasename, std::move(data))).second;
  } catch (input_parse_error&) {
    // elfSharedLibData ctor will throw if it is unable to parse input
    // just ignore it and don't add the library
    return false;
  }
}

bool ends_with(const char* str, const char* ending) {
  size_t str_len = strlen(str);
  size_t ending_len = strlen(ending);

  if (ending_len > str_len) {
    return false;
  }
  return strcmp(str + (str_len - ending_len), ending) == 0;
}

bool starts_with(char const* str, char const* start) {
  if (str == start) {
    return true;
  }

  while (*str && *str == *start) {
    ++str;
    ++start;
  }

  return *start == 0;
}

bool refresh_shared_lib_using_dl_iterate_phdr_if_can() {
  static auto dl_iterate_phdr =
    reinterpret_cast<int(*)(int(*)(dl_phdr_info*, size_t, void*), void*)>(
      dlsym(RTLD_DEFAULT, "dl_iterate_phdr"));

  if (dl_iterate_phdr == nullptr) {
    // Undefined symbol.
    return false;
  }

  dl_iterate_phdr(+[](dl_phdr_info* info, size_t, void*) {
    if (info->dlpi_name &&
          (ends_with(info->dlpi_name, ".so")  || starts_with(info->dlpi_name, "app_process"))) {
      addSharedLib(info->dlpi_addr, info->dlpi_name, info->dlpi_phdr, info->dlpi_phnum);
    }
    return 0;
  }, nullptr);

  return true;
}

} // namespace (anonymous)

LibLookupResult sharedLib(char const* libname) {
  char const* libbasename = basename(libname);
  auto result = [&]{
    // this lambda is to ensure that we only hold the lock for the lookup.
    // the boolean check outside this block transitively calls dladdr(3), which
    // would cause a lock inversion with refresh_shared_libs under Bionic
    ReaderLock rl(&sharedLibsMutex_);

    // std::unordered_map<>::at() seems broken for at least x86_64 in ndk17 - instead of throwing
    // an out_of_range exception for a not-found key, it segfaults. so, do our own search and check.
    auto iter = sharedLibData().find(libbasename);
    if (iter == sharedLibData().end()) {
      return LibLookupResult{.success = false};
    }
    return LibLookupResult{.success = true, .data = iter->second};
  }();
  if (!result.success || !result.data) { // this is necessary to ensure our data is still valid - lib might have been unloaded
    WriterLock wl(&sharedLibsMutex_);
    sharedLibData().erase(libbasename);
    return LibLookupResult{.success = false};
  }
  return result;
}

std::vector<std::pair<std::string, elfSharedLibData>> allSharedLibs() {
  ReaderLock rl(&sharedLibsMutex_);
  std::vector<std::pair<std::string, elfSharedLibData>> libs;
  libs.reserve(sharedLibData().size());
  std::copy(sharedLibData().begin(), sharedLibData().end(), std::back_inserter(libs));
  return libs;
}

// for testing. not exposed via header.
void clearSharedLibs() {
  WriterLock wl(&sharedLibsMutex_);
  sharedLibData().clear();
}

} } // namespace facebook::linker

extern "C" {

using namespace facebook::linker;
#define ANDROID_L  21

int
refresh_shared_libs() {
  bool successful = refresh_shared_lib_using_dl_iterate_phdr_if_can();
  if (successful) {
    return 0;
  }

#ifndef __LP64__ /* prior to android L there were no 64-bit devices anyway */
  auto androidSdk = facebook::build::getAndroidSdk();
  if (androidSdk < ANDROID_L) {
    // For some reason this can crash a lot on dalvik so we are only going to try if we can't
    // use some other method
    soinfo* si = reinterpret_cast<soinfo*>(dlopen(nullptr, RTLD_LOCAL));

    if (si == nullptr) {
      return 1;
    }

    for (; si != nullptr; si = si->next) {
      auto name = si->link_map.l_name;
      if (name && (ends_with(name, ".so") || starts_with(name, "app_process"))) {
        ElfW(Addr) base = 0;
        if (androidSdk >= 17) {
          base = si->load_bias;
        } else {
          base = si->base;
        }
        addSharedLib(base, name, si->phdr, si->phnum);
      }
    }

    return 0;
  }
#endif

  return successful ? 0 : 1;
}

} // extern C
