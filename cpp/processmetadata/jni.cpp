// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <fbjni/fbjni.h>
#include <fb/xplat_init.h>

#include <loom/processmetadata/ProcessMetadata.h>

using namespace facebook::loom;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    facebook::jni::registerNatives(
      "com/facebook/loom/provider/processmetadata/ProcessMetadataProvider",
      {
        makeNativeMethod("nativeLogProcessMetadata", processmetadata::logProcessMetadata),
      });
  });
}
