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

#include "thread.h"

#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <algorithm>
#include <bitset>
#include <cerrno>
#include <iostream>
#include <list>
#include <sstream>

#include "arch/context.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/bit_utils.h"
#include "base/memory_tool.h"
#include "base/mutex.h"
#include "base/timing_logger.h"
#include "base/to_str.h"
#include "base/systrace.h"
#include "class_linker-inl.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap-inl.h"
#include "gc/allocator/rosalloc.h"
#include "gc/heap.h"
#include "gc/space/space-inl.h"
#include "handle_scope-inl.h"
#include "indirect_reference_table-inl.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/stack_trace_element.h"
#include "monitor.h"
#include "oat_quick_method_header.h"
#include "object_lock.h"
#include "quick_exception_handler.h"
#include "quick/quick_method_frame_info.h"
#include "reflection.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
//#include "ScopedLocalRef.h"
//#include "ScopedUtfChars.h"
#include "stack.h"
#include "stack_map.h"
#include "thread_list.h"
#include "thread-inl.h"
#include "utils.h"
#include "verifier/method_verifier.h"
#include "verify_object-inl.h"
#include "well_known_classes.h"
#include "interpreter/interpreter.h"

#if ART_USE_FUTEXES
#include "linux/futex.h"
#include "sys/syscall.h"
#ifndef SYS_futex
#define SYS_futex __NR_futex
#endif
#endif  // ART_USE_FUTEXES

namespace art {

bool Thread::is_started_ = false;
pthread_key_t Thread::pthread_key_self_;
ConditionVariable* Thread::resume_cond_ = nullptr;
bool (*Thread::is_sensitive_thread_hook_)() = nullptr;
Thread* Thread::jit_sensitive_thread_ = nullptr;

static constexpr bool kVerifyImageObjectsMarked = kIsDebugBuild;

// For implicit overflow checks we reserve an extra piece of memory at the bottom
// of the stack (lowest memory).  The higher portion of the memory
// is protected against reads and the lower is available for use while
// throwing the StackOverflow exception.
constexpr size_t kStackOverflowProtectedSize = 4 * kMemoryToolStackGuardSizeScale * KB;

static const char* kThreadNameDuringStartup = "<native thread without managed peer>";

bool Thread::PassActiveSuspendBarriers(Thread* self) {
  // Grab the suspend_count lock and copy the current set of
  // barriers. Then clear the list and the flag. The ModifySuspendCount
  // function requires the lock so we prevent a race between setting
  // the kActiveSuspendBarrier flag and clearing it.
  AtomicInteger* pass_barriers[kMaxSuspendBarriers];
  {
    MutexLock mu(self, *Locks::thread_suspend_count_lock_);
    if (!ReadFlag(kActiveSuspendBarrier)) {
      // quick exit test: the barriers have already been claimed - this is
      // possible as there may be a race to claim and it doesn't matter
      // who wins.
      // All of the callers of this function (except the SuspendAllInternal)
      // will first test the kActiveSuspendBarrier flag without lock. Here
      // double-check whether the barrier has been passed with the
      // suspend_count lock.
      return false;
    }

    for (uint32_t i = 0; i < kMaxSuspendBarriers; ++i) {
      pass_barriers[i] = tlsPtr_.active_suspend_barriers[i];
      tlsPtr_.active_suspend_barriers[i] = nullptr;
    }
    AtomicClearFlag(kActiveSuspendBarrier);
  }

  uint32_t barrier_count = 0;
  for (uint32_t i = 0; i < kMaxSuspendBarriers; i++) {
    AtomicInteger* pending_threads = pass_barriers[i];
    if (pending_threads != nullptr) {
      bool done = false;
      do {
        int32_t cur_val = pending_threads->LoadRelaxed();
        CHECK_GT(cur_val, 0) << "Unexpected value for PassActiveSuspendBarriers(): " << cur_val;
        // Reduce value by 1.
        done = pending_threads->CompareExchangeWeakRelaxed(cur_val, cur_val - 1);
#if ART_USE_FUTEXES
        if (done && (cur_val - 1) == 0) {  // Weak CAS may fail spuriously.
          futex(pending_threads->Address(), FUTEX_WAKE, -1, nullptr, nullptr, 0);
        }
#endif
      } while (!done);
      ++barrier_count;
    }
  }
  CHECK_GT(barrier_count, 0U);
  return true;
}

void Thread::RunCheckpointFunction() {
  Closure *checkpoints[kMaxCheckpoints];

  // Grab the suspend_count lock and copy the current set of
  // checkpoints.  Then clear the list and the flag.  The RequestCheckpoint
  // function will also grab this lock so we prevent a race between setting
  // the kCheckpointRequest flag and clearing it.
  {
    MutexLock mu(this, *Locks::thread_suspend_count_lock_);
    for (uint32_t i = 0; i < kMaxCheckpoints; ++i) {
      checkpoints[i] = tlsPtr_.checkpoint_functions[i];
      tlsPtr_.checkpoint_functions[i] = nullptr;
    }
    AtomicClearFlag(kCheckpointRequest);
  }

  // Outside the lock, run all the checkpoint functions that
  // we collected.
  bool found_checkpoint = false;
  for (uint32_t i = 0; i < kMaxCheckpoints; ++i) {
    if (checkpoints[i] != nullptr) {
      ScopedTrace trace("Run checkpoint function");
      checkpoints[i]->Run(this);
      found_checkpoint = true;
    }
  }
  CHECK(found_checkpoint);
}

Closure* Thread::GetFlipFunction() {
  Atomic<Closure*>* atomic_func = reinterpret_cast<Atomic<Closure*>*>(&tlsPtr_.flip_function);
  Closure* func;
  do {
    func = atomic_func->LoadRelaxed();
    if (func == nullptr) {
      return nullptr;
    }
  } while (!atomic_func->CompareExchangeWeakSequentiallyConsistent(func, nullptr));
  DCHECK(func != nullptr);
  return func;
}

}  // namespace art
