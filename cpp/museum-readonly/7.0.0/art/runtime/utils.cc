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

#include <museum/7.0.0/art/runtime/utils.h>

#include <museum/7.0.0/bionic/libc/inttypes.h>
#include <museum/7.0.0/bionic/libc/pthread.h>
#include <museum/7.0.0/bionic/libc/sys/stat.h>
#include <museum/7.0.0/bionic/libc/sys/syscall.h>
#include <museum/7.0.0/bionic/libc/sys/types.h>
#include <museum/7.0.0/bionic/libc/sys/wait.h>
#include <museum/7.0.0/bionic/libc/unistd.h>
#include <museum/7.0.0/external/libcxx/memory>

#include <museum/7.0.0/art/runtime/art_field-inl.h>
#include <museum/7.0.0/art/runtime/art_method-inl.h>
#include <museum/7.0.0/art/runtime/base/stl_util.h>
#include <museum/7.0.0/art/runtime/base/unix_file/fd_file.h>
#include <museum/7.0.0/art/runtime/dex_file-inl.h>
#include <museum/7.0.0/art/runtime/dex_instruction.h>
#include <museum/7.0.0/art/runtime/mirror/class-inl.h>
#include <museum/7.0.0/art/runtime/mirror/class_loader.h>
#include <museum/7.0.0/art/runtime/mirror/object-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object_array-inl.h>
#include <museum/7.0.0/art/runtime/mirror/string.h>
#include <museum/7.0.0/art/runtime/oat_quick_method_header.h>
#include <museum/7.0.0/art/runtime/os.h>
#include <museum/7.0.0/art/runtime/scoped_thread_state_change.h>
#include <museum/7.0.0/art/runtime/utf-inl.h>

// For DumpNativeStack.
//#include <backtrace/Backtrace.h>
//#include <backtrace/BacktraceMap.h>

#if defined(__linux__)
#include <museum/7.0.0/bionic/libc/linux/unistd.h>
#endif

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

#if defined(__linux__)
static constexpr bool kUseAddr2line = !kIsTargetBuild;
#endif

pid_t GetTid() {
#if defined(__APPLE__)
  uint64_t owner;
  CHECK_PTHREAD_CALL(pthread_threadid_np, (nullptr, &owner), __FUNCTION__);  // Requires Mac OS 10.6
  return owner;
#elif defined(__BIONIC__)
  return gettid();
#else
  return syscall(__NR_gettid);
#endif
}

void SleepForever() {
  while (true) {
    usleep(1000000);
  }
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
