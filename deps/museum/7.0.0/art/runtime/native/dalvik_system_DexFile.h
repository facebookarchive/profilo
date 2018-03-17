/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_NATIVE_DALVIK_SYSTEM_DEXFILE_H_
#define ART_RUNTIME_NATIVE_DALVIK_SYSTEM_DEXFILE_H_

#include <jni.h>
#include <unistd.h>

namespace art {

constexpr size_t kOatFileIndex = 0;
constexpr size_t kDexFileIndexStart = 1;

class DexFile;

void register_dalvik_system_DexFile(JNIEnv* env);

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_DALVIK_SYSTEM_DEXFILE_H_
