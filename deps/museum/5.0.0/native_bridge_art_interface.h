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

#ifndef ART_RUNTIME_NATIVE_BRIDGE_ART_INTERFACE_H_
#define ART_RUNTIME_NATIVE_BRIDGE_ART_INTERFACE_H_

#include <jni.h>
#include <stdint.h>
#include <string>

namespace art {

// Mirror libnativebridge interface. Done to have the ART callbacks out of line, and not require
// the system/core header file in other files.

void LoadNativeBridge(std::string& native_bridge_library_filename);

// This is mostly for testing purposes, as in a full system this is called by Zygote code.
void PreInitializeNativeBridge(std::string dir);

void InitializeNativeBridge(JNIEnv* env, const char* instruction_set);

void UnloadNativeBridge();

};  // namespace art

#endif  // ART_RUNTIME_NATIVE_BRIDGE_ART_INTERFACE_H_
