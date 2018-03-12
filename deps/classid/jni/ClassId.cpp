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

#include <fb/fbjni.h>
#include <fb/fbjni/ByteBuffer.h>
#include <fb/xplat_init.h>

using namespace facebook::jni;

namespace facebook {
namespace classid {
namespace {

const char* ClassIdType = "com/facebook/common/dextricks/classid/ClassId";

class DexFileBits {
 public:
  // vtable from virtual destructor in DexFile on Oreo
  void* vtable;

  // The base address of the memory mapping.
  uint8_t* begin;

  // The size of the underlying memory allocation in bytes.
  size_t size;
};

static jint getSignatureFromDexFile(alias_ref<jobject>, jlong dexFilePointer) {
  const DexFileBits* dexFile = (const DexFileBits*)dexFilePointer;
  if (dexFile->size < 16) {
    return 0;
  }

  uint8_t* signatureStart = dexFile->begin + 12;
  int32_t signature = *(int32_t*)signatureStart;
  return signature;
}

static jint getSignatureFromDexData(
    alias_ref<jobject>,
    alias_ref<JByteBuffer> dexData) {
  if (dexData.get() == nullptr) {
    return 0;
  }

  if (dexData->isDirect()) {
    if (dexData->getDirectSize() < 16) {
      return 0;
    }

    uint8_t* signatureStart = dexData->getDirectBytes() + 12;
    int32_t signature = *(int32_t*)signatureStart;
    return signature;
  }

  return 0;
}

} // namespace
} // namespace classid
} // namespace facebook

using namespace facebook;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return xplat::initialize(vm, [] {
    registerNatives(
        classid::ClassIdType,
        {
            makeNativeMethod(
                "getSignatureFromDexFile", classid::getSignatureFromDexFile),
            makeNativeMethod(
                "getSignatureFromDexData", classid::getSignatureFromDexData),
        });
  });
}
