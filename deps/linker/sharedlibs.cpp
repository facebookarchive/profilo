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

#include <fb/Build.h>

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
  if (sharedLibData().find(basename(libname)) == sharedLibData().end()) {
    try {
      elfSharedLibData data(std::forward<Arg>(arg));
      WriterLock wl(&sharedLibsMutex_);
      return sharedLibData().insert(std::make_pair(basename(libname), std::move(data))).second;
    } catch (input_parse_error&) {
      // elfSharedLibData ctor will throw if it is unable to parse input
      // just ignore it and don't add the library, we'll return false
    }
  }
  return false;
}

bool ends_with(const char* str, const char* ending) {
  size_t str_len = strlen(str);
  size_t ending_len = strlen(ending);

  if (ending_len > str_len) {
    return false;
  }
  return strcmp(str + (str_len - ending_len), ending) == 0;
}

} // namespace (anonymous)

elfSharedLibData sharedLib(char const* libname) {
  ReaderLock rl(&sharedLibsMutex_);
  auto lib = sharedLibData().at(basename(libname));
  if (!lib) {
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
  if (facebook::build::Build::getAndroidSdk() >= ANDROID_L) {
    static auto dl_iterate_phdr =
      reinterpret_cast<int(*)(int(*)(dl_phdr_info*, size_t, void*), void*)>(
        dlsym(RTLD_DEFAULT, "dl_iterate_phdr"));

    if (dl_iterate_phdr == nullptr) {
      // Undefined symbol.
      return 1;
    }

    dl_iterate_phdr(+[](dl_phdr_info* info, size_t, void*) {
      if (info->dlpi_name && ends_with(info->dlpi_name, ".so")) {
        addSharedLib(info->dlpi_name, info);
      }
      return 0;
    }, nullptr);
  } else {
    soinfo* si = reinterpret_cast<soinfo*>(dlopen(nullptr, RTLD_LOCAL));

    if (si == nullptr) {
      return 1;
    }

    for (; si != nullptr; si = si->next) {
      if (si->link_map.l_name && ends_with(si->link_map.l_name, ".so")) {
        addSharedLib(si->link_map.l_name, si);
      }
    }
  }
  return 0;
}

} // extern C
