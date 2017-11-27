// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <fb/fbjni.h>
#include <fb/xplat_init.h>

#include "Atrace.h"

using namespace facebook::loom;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    atrace::registerNatives();
  });
}
