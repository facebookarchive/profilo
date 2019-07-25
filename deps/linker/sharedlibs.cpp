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

#include <sys/system_properties.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unordered_map>
#include <libgen.h>

#define DT_GNU_HASH	0x6ffffef5	/* GNU-style hash table.  */

namespace facebook {
namespace linker {

namespace {

static pthread_rwlock_t sharedLibsMutex_ = PTHREAD_RWLOCK_INITIALIZER;
static std::unordered_map<std::string, elfSharedLibData>& sharedLibData() {
  static std::unordered_map<std::string, elfSharedLibData> sharedLibData_;
  return sharedLibData_;
}

template <typename Arg>
bool addSharedLib(char const* libname, Arg&& arg) {
  {
    ReaderLock rl(&sharedLibsMutex_);
    // the common path will be duplicate entries, so skip the weight of
    // elfSharedLibData construction and grabbing a writer lock, if possible
    if (sharedLibData().find(basename(libname)) != sharedLibData().end()) {
      return false;
    }
  }

  try {
    elfSharedLibData data(std::forward<Arg>(arg));
    WriterLock wl(&sharedLibsMutex_);
    return sharedLibData().insert(std::make_pair(basename(libname), std::move(data))).second;
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

} // namespace (anonymous)

elfSharedLibData sharedLib(char const* libname) {
  char const* libbasename = basename(libname);
  auto lib = [&]{
    // this lambda is to ensure that we only hold the lock for the lookup.
    // the boolean check outside this block transitively calls dladdr(3), which
    // would cause a lock inversion with refresh_shared_libs under Bionic
    ReaderLock rl(&sharedLibsMutex_);

    // std::unordered_map<>::at() seems broken for at least x86_64 in ndk17 - instead of throwing
    // an out_of_range exception for a not-found key, it segfaults. so, do our own search and check.
    auto iter = sharedLibData().find(libbasename);
    if (iter == sharedLibData().end()) {
      throw std::out_of_range(libname);
    }
    return iter->second;
  }();
  if (!lib) { // this is necessary to ensure our data is still valid - lib might have been unloaded
    WriterLock wl(&sharedLibsMutex_);
    sharedLibData().erase(libbasename);
    throw std::out_of_range(libname);
  }
  return lib;
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
  if (facebook::build::getAndroidSdk() >= ANDROID_L) {
    static auto dl_iterate_phdr =
      reinterpret_cast<int(*)(int(*)(dl_phdr_info*, size_t, void*), void*)>(
        dlsym(RTLD_DEFAULT, "dl_iterate_phdr"));

    if (dl_iterate_phdr == nullptr) {
      // Undefined symbol.
      return 1;
    }

    dl_iterate_phdr(+[](dl_phdr_info* info, size_t, void*) {
      if (info->dlpi_name &&
            (ends_with(info->dlpi_name, ".so")  || starts_with(info->dlpi_name, "app_process"))) {
        addSharedLib(info->dlpi_name, info);
      }
      return 0;
    }, nullptr);
  } else {
#ifndef __LP64__ /* prior to android L there were no 64-bit devices anyway */
    soinfo* si = reinterpret_cast<soinfo*>(dlopen(nullptr, RTLD_LOCAL));

    if (si == nullptr) {
      return 1;
    }

    for (; si != nullptr; si = si->next) {
      if (si->link_map.l_name &&
            (ends_with(si->link_map.l_name, ".so") || starts_with(si->link_map.l_name, "app_process"))) {
        addSharedLib(si->link_map.l_name, si);
      }
    }
#endif
  }
  return 0;
}

} // extern C
