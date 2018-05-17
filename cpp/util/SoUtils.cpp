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

#include "SoUtils.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <sys/system_properties.h>

#define _Bool bool
#include <linker/bionic_linker.h>

namespace facebook {
namespace profilo {
namespace util {

namespace {

int get_android_sdk() {
  char sdk_version_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.sdk", sdk_version_str);
  return atoi(sdk_version_str);
}

} // namespace

void* resolve_symbol(const char* name) {
  if (get_android_sdk() >= 21 /* LOLLIPOP */) {
    return dlsym(RTLD_DEFAULT, name);
  } else {
    // For SDK < 21 Linker can crash with SIGFPE on dlsym(RTLD_DEFAULT, ...).
    // The SIGFPE is caused by a divide-by-zero error if si->nbucket_ is 0,
    // so we avoid this condition.
    // https://code.google.com/p/android/issues/detail?id=61799
    soinfo* si = reinterpret_cast<soinfo*>(dlopen(nullptr, RTLD_LOCAL));
    for (; si != nullptr; si = si->next) {
      if (!si->link_map.l_name || si->nbucket == 0) {
        continue;
      }
        auto ptr = dlsym(si, name);
        if (ptr) {
          return ptr;
        }
    }
  }
  return nullptr;
}

} // util
} // profilo
} // facebook
