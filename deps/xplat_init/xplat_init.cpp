// Copyright 2004-present Facebook. All Rights Reserved.

#include <fb/Environment.h>
#include <fb/fbjni.h>

namespace facebook { namespace xplat {

__attribute__((visibility("default")))
jint initialize(JavaVM* vm, void(*init_fn)()) noexcept {
  return jni::initialize(vm, [init_fn] {
      init_fn();
  });
}

}
}

