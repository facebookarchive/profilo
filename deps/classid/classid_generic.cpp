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

#include <museum/museum_header.h>

#if defined(MUSEUM_VERSION_8_0_0)
#include <museum/8.0.0/art/runtime/dex_file.h>
#elif defined(MUSEUM_VERSION_8_1_0)
#include <museum/8.1.0/art/runtime/dex_file.h>
#elif defined(MUSEUM_VERSION_9_0_0)
#include <museum/9.0.0/art/libdexfile/dex/dex_file.h>
#endif

#include <fb/fbjni.h>
#include <fb/fbjni/ByteBuffer.h>
#include <fb/xplat_init.h>
#include <sig_safe_write/sig_safe_write.h>

using namespace facebook::jni;

namespace facebook {
namespace classid {

using namespace museum::MUSEUM_VERSION::art;

struct GetSignatureData {
  const jlong inDexFilePointer;
  jint outSignature;
};

static void GetSignatureOp(void* data) {
  GetSignatureData* params = (GetSignatureData*) data;

  const DexFile* dexFile = (const DexFile*)params->inDexFilePointer;
  const DexFile::Header& header = dexFile->GetHeader();

  params->outSignature = *(int32_t*)header.signature_;
}

#if defined(MUSEUM_VERSION_8_0_0) || \
    defined(MUSEUM_VERSION_8_1_0) || \
    defined(MUSEUM_VERSION_9_0_0)

JNIEXPORT jint
#if defined(MUSEUM_VERSION_8_0_0)
getSignatureFromDexFile_8_0_0(alias_ref<jobject>, jlong dexFilePointer)
#elif defined(MUSEUM_VERSION_8_1_0)
getSignatureFromDexFile_8_1_0(alias_ref<jobject>, jlong dexFilePointer)
#elif defined(MUSEUM_VERSION_9_0_0)
getSignatureFromDexFile_9_0_0(alias_ref<jobject>, jlong dexFilePointer)
#endif
{
  GetSignatureData data {
    .inDexFilePointer = dexFilePointer,
  };

  if (sig_safe_op(&GetSignatureOp, &data)) {
    return 0;
  } else {
    return data.outSignature;
  }
}

#endif

} // namespace classid
} // namespace facebook
