/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_JNI_INTERNAL_H_
#define ART_RUNTIME_JNI_INTERNAL_H_

#include <jni.h>
#include <iosfwd>

#include "base/macros.h"

namespace art {

class ArtField;
class ArtMethod;

const JNINativeInterface* GetJniNativeInterface();
const JNINativeInterface* GetRuntimeShutdownNativeInterface();

int ThrowNewException(JNIEnv* env, jclass exception_class, const char* msg, jobject cause);

namespace jni {

ALWAYS_INLINE
static inline ArtField* DecodeArtField(jfieldID fid) {
  return reinterpret_cast<ArtField*>(fid);
}

ALWAYS_INLINE
static inline jfieldID EncodeArtField(ArtField* field) {
  return reinterpret_cast<jfieldID>(field);
}

ALWAYS_INLINE
static inline jmethodID EncodeArtMethod(ArtMethod* art_method) {
  return reinterpret_cast<jmethodID>(art_method);
}

ALWAYS_INLINE
static inline ArtMethod* DecodeArtMethod(jmethodID method_id) {
  return reinterpret_cast<ArtMethod*>(method_id);
}

}  // namespace jni
}  // namespace art

std::ostream& operator<<(std::ostream& os, const jobjectRefType& rhs);

#endif  // ART_RUNTIME_JNI_INTERNAL_H_
