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

#ifndef NATIVE_METHOD
#define NATIVE_METHOD(className, functionName, signature) \
  { #functionName, signature, reinterpret_cast<void*>(className ## _ ## functionName) }
#endif

// TODO: Can we do a better job of supporting overloading ?
#ifndef OVERLOADED_NATIVE_METHOD
#define OVERLOADED_NATIVE_METHOD(className, functionName, signature, identifier) \
    { #functionName, signature, reinterpret_cast<void*>(className ## _ ## identifier) }
#endif

#define REGISTER_NATIVE_METHODS(jni_class_name) \
  RegisterNativeMethods(env, jni_class_name, gMethods, arraysize(gMethods))

namespace art {

const JNINativeInterface* GetJniNativeInterface();
const JNINativeInterface* GetRuntimeShutdownNativeInterface();

// Similar to RegisterNatives except its passed a descriptor for a class name and failures are
// fatal.
void RegisterNativeMethods(JNIEnv* env, const char* jni_class_name, const JNINativeMethod* methods,
                           jint method_count);

int ThrowNewException(JNIEnv* env, jclass exception_class, const char* msg, jobject cause);

}  // namespace art

std::ostream& operator<<(std::ostream& os, const jobjectRefType& rhs);

#endif  // ART_RUNTIME_JNI_INTERNAL_H_
