/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_NATIVE_STACK_DUMP_H_
#define ART_RUNTIME_NATIVE_STACK_DUMP_H_

#include <unistd.h>

#include <iosfwd>

#include "base/macros.h"

class BacktraceMap;

namespace art {

class ArtMethod;

// Dumps the native stack for thread 'tid' to 'os'.
void DumpNativeStack(std::ostream& os,
                     pid_t tid,
                     BacktraceMap* map = nullptr,
                     const char* prefix = "",
                     ArtMethod* current_method = nullptr,
                     void* ucontext = nullptr)
    NO_THREAD_SAFETY_ANALYSIS;

// Dumps the kernel stack for thread 'tid' to 'os'. Note that this is only available on linux-x86.
void DumpKernelStack(std::ostream& os,
                     pid_t tid,
                     const char* prefix = "",
                     bool include_count = true);

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_STACK_DUMP_H_
