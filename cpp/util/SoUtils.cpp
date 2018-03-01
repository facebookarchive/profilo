// Copyright 2004-present Facebook. All Rights Reserved.

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
      if (!si->link_map_head.l_name || si->nbucket_ == 0) {
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
