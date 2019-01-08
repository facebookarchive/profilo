// Copyright 2004-present Facebook. All Rights Reserved.

#include <stdlib.h>
#include <string.h>

#include <sys/system_properties.h>

namespace facebook {
namespace build {

int getAndroidSdk() {
  static auto android_sdk = ([] {
     char sdk_version_str[PROP_VALUE_MAX];
     __system_property_get("ro.build.version.sdk", sdk_version_str);
     return atoi(sdk_version_str);
  })();

  return android_sdk;
}

bool isArt() {
  int sdk = getAndroidSdk();
  if (sdk >= 21) { // Lollipop (5.0)
    return true;
  } else if (sdk <= 18) { // Jelly Bean (4.3)
    return false;
  } else { // Kitkat (4.4/4.4W) - api 19 and 20
    static bool running_art = ([] {
      char vm_lib_str[PROP_VALUE_MAX];
      __system_property_get("persist.sys.dalvik.vm.lib", vm_lib_str);
      return !strncmp("libart", vm_lib_str, 6);
    })();

    return running_art;
  }
}

bool isDalvik() {
  return !isArt();
}

} // build
} // facebook
