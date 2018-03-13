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

#ifndef ART_RUNTIME_THREAD_INL_H_
#define ART_RUNTIME_THREAD_INL_H_

#include "thread.h"

#ifdef __ANDROID__
#include <bionic_tls.h>  // Access to our own TLS slot.
#endif

#include <pthread.h>

#include "base/casts.h"
#include "base/mutex-inl.h"
#include "gc/heap.h"
#include "jni_env_ext.h"
#include "thread_pool.h"

namespace art {

// Quickly access the current thread from a JNIEnv.
static inline Thread* ThreadForEnv(JNIEnv* env) {
  JNIEnvExt* full_env(down_cast<JNIEnvExt*>(env));
  return full_env->self;
}

inline Thread* Thread::Current() {
  // We rely on Thread::Current returning null for a detached thread, so it's not obvious
  // that we can replace this with a direct %fs access on x86.
  if (!is_started_) {
    return nullptr;
  } else {
#ifdef __ANDROID__
    void* thread = __get_tls()[TLS_SLOT_ART_THREAD_SELF];
#else
    void* thread = pthread_getspecific(Thread::pthread_key_self_);
#endif
    return reinterpret_cast<Thread*>(thread);
  }
}

inline void Thread::AllowThreadSuspension() {
  DCHECK_EQ(Thread::Current(), this);
  if (UNLIKELY(TestAllFlags())) {
    CheckSuspend();
  }
}

inline void Thread::CheckSuspend() {
  DCHECK_EQ(Thread::Current(), this);
  for (;;) {
    if (ReadFlag(kCheckpointRequest)) {
      RunCheckpointFunction();
    } else if (ReadFlag(kSuspendRequest)) {
      FullSuspendCheck();
    } else {
      break;
    }
  }
}

inline ThreadState Thread::SetState(ThreadState new_state) {
  // Should only be used to change between suspended states.
  // Cannot use this code to change into or from Runnable as changing to Runnable should
  // fail if old_state_and_flags.suspend_request is true and changing from Runnable might
  // miss passing an active suspend barrier.
  DCHECK_NE(new_state, kRunnable);
  if (kIsDebugBuild && this != Thread::Current()) {
    std::string name;
    GetThreadName(name);
    LOG(FATAL) << "Thread \"" << name << "\"(" << this << " != Thread::Current()="
               << Thread::Current() << ") changing state to " << new_state;
  }
  union StateAndFlags old_state_and_flags;
  old_state_and_flags.as_int = tls32_.state_and_flags.as_int;
  CHECK_NE(old_state_and_flags.as_struct.state, kRunnable);
  tls32_.state_and_flags.as_struct.state = new_state;
  return static_cast<ThreadState>(old_state_and_flags.as_struct.state);
}

inline void Thread::AssertThreadSuspensionIsAllowable(bool check_locks) const {
  if (kIsDebugBuild) {
    if (gAborting == 0) {
      CHECK_EQ(0u, tls32_.no_thread_suspension) << tlsPtr_.last_no_thread_suspension_cause;
    }
    if (check_locks) {
      bool bad_mutexes_held = false;
      for (int i = kLockLevelCount - 1; i >= 0; --i) {
        // We expect no locks except the mutator_lock_ or thread list suspend thread lock.
        if (i != kMutatorLock) {
          BaseMutex* held_mutex = GetHeldMutex(static_cast<LockLevel>(i));
          if (held_mutex != nullptr) {
            LOG(ERROR) << "holding \"" << held_mutex->GetName()
                      << "\" at point where thread suspension is expected";
            bad_mutexes_held = true;
          }
        }
      }
      if (gAborting == 0) {
        CHECK(!bad_mutexes_held);
      }
    }
  }
}

inline void Thread::TransitionToSuspendedAndRunCheckpoints(ThreadState new_state) {
  DCHECK_NE(new_state, kRunnable);
  DCHECK_EQ(GetState(), kRunnable);
  union StateAndFlags old_state_and_flags;
  union StateAndFlags new_state_and_flags;
  while (true) {
    old_state_and_flags.as_int = tls32_.state_and_flags.as_int;
    if (UNLIKELY((old_state_and_flags.as_struct.flags & kCheckpointRequest) != 0)) {
      RunCheckpointFunction();
      continue;
    }
    // Change the state but keep the current flags (kCheckpointRequest is clear).
    DCHECK_EQ((old_state_and_flags.as_struct.flags & kCheckpointRequest), 0);
    new_state_and_flags.as_struct.flags = old_state_and_flags.as_struct.flags;
    new_state_and_flags.as_struct.state = new_state;

    // CAS the value with a memory ordering.
    bool done =
        tls32_.state_and_flags.as_atomic_int.CompareExchangeWeakRelease(old_state_and_flags.as_int,
                                                                        new_state_and_flags.as_int);
    if (LIKELY(done)) {
      break;
    }
  }
}

inline void Thread::PassActiveSuspendBarriers() {
  while (true) {
    uint16_t current_flags = tls32_.state_and_flags.as_struct.flags;
    if (LIKELY((current_flags & (kCheckpointRequest | kActiveSuspendBarrier)) == 0)) {
      break;
    } else if ((current_flags & kActiveSuspendBarrier) != 0) {
      PassActiveSuspendBarriers(this);
    } else {
      // Impossible
      LOG(FATAL) << "Fatal, thread transitioned into suspended without running the checkpoint";
    }
  }
}

inline void Thread::TransitionFromRunnableToSuspended(ThreadState new_state) {
  AssertThreadSuspensionIsAllowable();
  DCHECK_EQ(this, Thread::Current());
  // Change to non-runnable state, thereby appearing suspended to the system.
  TransitionToSuspendedAndRunCheckpoints(new_state);
  // Mark the release of the share of the mutator_lock_.
  Locks::mutator_lock_->TransitionFromRunnableToSuspended(this);
  // Once suspended - check the active suspend barrier flag
  PassActiveSuspendBarriers();
}

inline ThreadState Thread::TransitionFromSuspendedToRunnable() {
  union StateAndFlags old_state_and_flags;
  old_state_and_flags.as_int = tls32_.state_and_flags.as_int;
  int16_t old_state = old_state_and_flags.as_struct.state;
  DCHECK_NE(static_cast<ThreadState>(old_state), kRunnable);
  do {
    Locks::mutator_lock_->AssertNotHeld(this);  // Otherwise we starve GC..
    old_state_and_flags.as_int = tls32_.state_and_flags.as_int;
    DCHECK_EQ(old_state_and_flags.as_struct.state, old_state);
    if (LIKELY(old_state_and_flags.as_struct.flags == 0)) {
      // Optimize for the return from native code case - this is the fast path.
      // Atomically change from suspended to runnable if no suspend request pending.
      union StateAndFlags new_state_and_flags;
      new_state_and_flags.as_int = old_state_and_flags.as_int;
      new_state_and_flags.as_struct.state = kRunnable;
      // CAS the value with a memory barrier.
      if (LIKELY(tls32_.state_and_flags.as_atomic_int.CompareExchangeWeakAcquire(
                                                 old_state_and_flags.as_int,
                                                 new_state_and_flags.as_int))) {
        // Mark the acquisition of a share of the mutator_lock_.
        Locks::mutator_lock_->TransitionFromSuspendedToRunnable(this);
        break;
      }
    } else if ((old_state_and_flags.as_struct.flags & kActiveSuspendBarrier) != 0) {
      PassActiveSuspendBarriers(this);
    } else if ((old_state_and_flags.as_struct.flags & kCheckpointRequest) != 0) {
      // Impossible
      LOG(FATAL) << "Transitioning to runnable with checkpoint flag, "
                 << " flags=" << old_state_and_flags.as_struct.flags
                 << " state=" << old_state_and_flags.as_struct.state;
    } else if ((old_state_and_flags.as_struct.flags & kSuspendRequest) != 0) {
      // Wait while our suspend count is non-zero.
      MutexLock mu(this, *Locks::thread_suspend_count_lock_);
      old_state_and_flags.as_int = tls32_.state_and_flags.as_int;
      DCHECK_EQ(old_state_and_flags.as_struct.state, old_state);
      while ((old_state_and_flags.as_struct.flags & kSuspendRequest) != 0) {
        // Re-check when Thread::resume_cond_ is notified.
        Thread::resume_cond_->Wait(this);
        old_state_and_flags.as_int = tls32_.state_and_flags.as_int;
        DCHECK_EQ(old_state_and_flags.as_struct.state, old_state);
      }
      DCHECK_EQ(GetSuspendCount(), 0);
    }
  } while (true);
  // Run the flip function, if set.
  Closure* flip_func = GetFlipFunction();
  if (flip_func != nullptr) {
    flip_func->Run(this);
  }
  return static_cast<ThreadState>(old_state);
}

inline void Thread::VerifyStack() {
  if (kVerifyStack) {
    if (Runtime::Current()->GetHeap()->IsObjectValidationEnabled()) {
      VerifyStackImpl();
    }
  }
}

inline size_t Thread::TlabSize() const {
  return tlsPtr_.thread_local_end - tlsPtr_.thread_local_pos;
}

inline mirror::Object* Thread::AllocTlab(size_t bytes) {
  DCHECK_GE(TlabSize(), bytes);
  ++tlsPtr_.thread_local_objects;
  mirror::Object* ret = reinterpret_cast<mirror::Object*>(tlsPtr_.thread_local_pos);
  tlsPtr_.thread_local_pos += bytes;
  return ret;
}

inline bool Thread::PushOnThreadLocalAllocationStack(mirror::Object* obj) {
  DCHECK_LE(tlsPtr_.thread_local_alloc_stack_top, tlsPtr_.thread_local_alloc_stack_end);
  if (tlsPtr_.thread_local_alloc_stack_top < tlsPtr_.thread_local_alloc_stack_end) {
    // There's room.
    DCHECK_LE(reinterpret_cast<uint8_t*>(tlsPtr_.thread_local_alloc_stack_top) +
              sizeof(StackReference<mirror::Object>),
              reinterpret_cast<uint8_t*>(tlsPtr_.thread_local_alloc_stack_end));
    DCHECK(tlsPtr_.thread_local_alloc_stack_top->AsMirrorPtr() == nullptr);
    tlsPtr_.thread_local_alloc_stack_top->Assign(obj);
    ++tlsPtr_.thread_local_alloc_stack_top;
    return true;
  }
  return false;
}

inline void Thread::SetThreadLocalAllocationStack(StackReference<mirror::Object>* start,
                                                  StackReference<mirror::Object>* end) {
  DCHECK(Thread::Current() == this) << "Should be called by self";
  DCHECK(start != nullptr);
  DCHECK(end != nullptr);
  DCHECK_ALIGNED(start, sizeof(StackReference<mirror::Object>));
  DCHECK_ALIGNED(end, sizeof(StackReference<mirror::Object>));
  DCHECK_LT(start, end);
  tlsPtr_.thread_local_alloc_stack_end = end;
  tlsPtr_.thread_local_alloc_stack_top = start;
}

inline void Thread::RevokeThreadLocalAllocationStack() {
  if (kIsDebugBuild) {
    // Note: self is not necessarily equal to this thread since thread may be suspended.
    Thread* self = Thread::Current();
    DCHECK(this == self || IsSuspended() || GetState() == kWaitingPerformingGc)
        << GetState() << " thread " << this << " self " << self;
  }
  tlsPtr_.thread_local_alloc_stack_end = nullptr;
  tlsPtr_.thread_local_alloc_stack_top = nullptr;
}

}  // namespace art

#endif  // ART_RUNTIME_THREAD_INL_H_
