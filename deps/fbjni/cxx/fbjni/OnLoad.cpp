/*
 * Copyright (c) 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <fbjni/fbjni.h>
#include <fbjni/NativeRunnable.h>

using namespace facebook::jni;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  return facebook::jni::initialize(vm, [] {
    HybridDataOnLoad();
    JNativeRunnable::OnLoad();
    ThreadScope::OnLoad();
  });
}
