// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <fbjni/fbjni.h>
#include <fb/xplat_init.h>

#include "SystemCounterThread.h"

using namespace facebook::loom;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    SystemCounterThread::registerNatives();
  });
}
