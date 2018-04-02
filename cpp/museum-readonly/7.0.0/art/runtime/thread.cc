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

#include <museum/7.0.0/art/runtime/thread.h>

#include <museum/7.0.0/bionic/libc/pthread.h>
#include <museum/7.0.0/bionic/libc/signal.h>
#include <museum/7.0.0/bionic/libc/sys/resource.h>
#include <museum/7.0.0/bionic/libc/sys/time.h>

#include <museum/7.0.0/external/libcxx/algorithm>
#include <museum/7.0.0/external/libcxx/bitset>
#include <museum/7.0.0/external/libcxx/cerrno>
#include <museum/7.0.0/external/libcxx/iostream>
#include <museum/7.0.0/external/libcxx/list>
#include <museum/7.0.0/external/libcxx/sstream>

#include <museum/7.0.0/art/runtime/arch/context.h>
#include <museum/7.0.0/art/runtime/art_field-inl.h>
#include <museum/7.0.0/art/runtime/art_method-inl.h>
#include <museum/7.0.0/art/runtime/base/bit_utils.h>
#include <museum/7.0.0/art/runtime/base/memory_tool.h>
#include <museum/7.0.0/art/runtime/base/mutex.h>
#include <museum/7.0.0/art/runtime/base/timing_logger.h>
#include <museum/7.0.0/art/runtime/base/to_str.h>
#include <museum/7.0.0/art/runtime/base/systrace.h>
#include <museum/7.0.0/art/runtime/class_linker-inl.h>
#include <museum/7.0.0/art/runtime/debugger.h>
#include <museum/7.0.0/art/runtime/dex_file-inl.h>
#include <museum/7.0.0/art/runtime/entrypoints/entrypoint_utils.h>
#include <museum/7.0.0/art/runtime/entrypoints/quick/quick_alloc_entrypoints.h>
#include <museum/7.0.0/art/runtime/gc/accounting/card_table-inl.h>
#include <museum/7.0.0/art/runtime/gc/accounting/heap_bitmap-inl.h>
#include <museum/7.0.0/art/runtime/gc/allocator/rosalloc.h>
#include <museum/7.0.0/art/runtime/gc/heap.h>
#include <museum/7.0.0/art/runtime/gc/space/space-inl.h>
#include <museum/7.0.0/art/runtime/handle_scope-inl.h>
#include <museum/7.0.0/art/runtime/indirect_reference_table-inl.h>
#include <museum/7.0.0/art/runtime/jni_internal.h>
#include <museum/7.0.0/art/runtime/mirror/class_loader.h>
#include <museum/7.0.0/art/runtime/mirror/class-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object_array-inl.h>
#include <museum/7.0.0/art/runtime/mirror/stack_trace_element.h>
#include <museum/7.0.0/art/runtime/monitor.h>
#include <museum/7.0.0/art/runtime/oat_quick_method_header.h>
#include <museum/7.0.0/art/runtime/object_lock.h>
#include <museum/7.0.0/art/runtime/quick_exception_handler.h>
#include <museum/7.0.0/art/runtime/quick/quick_method_frame_info.h>
#include <museum/7.0.0/art/runtime/reflection.h>
#include <museum/7.0.0/art/runtime/runtime.h>
#include <museum/7.0.0/art/runtime/scoped_thread_state_change.h>
//#include <museum/7.0.0/art/runtime/ScopedLocalRef.h>
//#include <museum/7.0.0/art/runtime/ScopedUtfChars.h>
#include <museum/7.0.0/art/runtime/stack.h>
#include <museum/7.0.0/art/runtime/stack_map.h>
#include <museum/7.0.0/art/runtime/thread_list.h>
#include <museum/7.0.0/art/runtime/thread-inl.h>
#include <museum/7.0.0/art/runtime/utils.h>
#include <museum/7.0.0/art/runtime/verifier/method_verifier.h>
#include <museum/7.0.0/art/runtime/verify_object-inl.h>
#include <museum/7.0.0/art/runtime/well_known_classes.h>
#include <museum/7.0.0/art/runtime/interpreter/interpreter.h>

#if ART_USE_FUTEXES
#include <museum/7.0.0/bionic/libc/linux/futex.h>
#include <museum/7.0.0/bionic/libc/sys/syscall.h>
#ifndef SYS_futex
#define SYS_futex __NR_futex
#endif
#endif  // ART_USE_FUTEXES

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

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

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
