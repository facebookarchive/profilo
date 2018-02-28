// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <fbjni/fbjni.h>
#include <fb/xplat_init.h>

#include <profilo/threadmetadata/ThreadMetadata.h>

using namespace facebook::profilo;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    facebook::jni::registerNatives(
      "com/facebook/profilo/provider/threadmetadata/ThreadMetadataProvider",
      {
        makeNativeMethod("nativeLogThreadMetadata", threadmetadata::logThreadMetadata),
      });
  });
}
