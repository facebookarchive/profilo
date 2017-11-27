// Copyright 2004-present Facebook. All Rights Reserved.

#include <fb/fbjni.h>
#include <fb/xplat_init.h>

using namespace facebook::jni;

namespace facebook {
namespace classid {
namespace {

const char *ClassIdType = "com/facebook/common/dextricks/classid/ClassId";

class DexFileBits {
public:
  // not in the AOSP, but in practice it exists on Pixel devices, vtable?
  uint8_t padding[4];

  // The base address of the memory mapping.
  uint8_t *begin;

  // The size of the underlying memory allocation in bytes.
  size_t size;
};

static jint getSignatureFromDexFile(alias_ref<jobject>, jlong dexFilePointer) {
  const DexFileBits *dexFile = (const DexFileBits *)dexFilePointer;
  if (dexFile->size < 16) {
    return 0;
  }

  uint8_t *signatureStart = dexFile->begin + 12;
  int32_t signature = *(int32_t *)signatureStart;
  return signature;
}

} // namespace
} // namespace classid
} // namespace facebook

using namespace facebook;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return xplat::initialize(vm, [] {
    registerNatives(classid::ClassIdType,
                    {
                        makeNativeMethod("getSignatureFromDexFile",
                                         classid::getSignatureFromDexFile),
                    });
  });
}
