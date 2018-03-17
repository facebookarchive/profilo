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

#pragma once

#include <museum/7.0.0/libnativehelper/jni.h>
#include <museum/7.0.0/bionic/libc/unistd.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace entrypoints {

struct JavaFrame {
  uint32_t methodIdx;
  uint32_t dexSignature;
};

void InitRuntime();
void InstallRuntime(JNIEnv* env, void* thread);
size_t GetStackTrace(JavaFrame* frames, size_t max_frames, void* thread);

} // namespace entrypoints
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
