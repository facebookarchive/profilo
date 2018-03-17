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

#include "utils.h"

#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "dex_file-inl.h"
#include "dex_instruction.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string.h"
#include "oat_quick_method_header.h"
#include "os.h"
#include "scoped_thread_state_change.h"
#include "utf-inl.h"

// For DumpNativeStack.
//#include <backtrace/Backtrace.h>
//#include <backtrace/BacktraceMap.h>

#if defined(__linux__)
#include <linux/unistd.h>
#endif

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
