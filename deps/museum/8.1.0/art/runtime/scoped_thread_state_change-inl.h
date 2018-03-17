/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_SCOPED_THREAD_STATE_CHANGE_INL_H_
#define ART_RUNTIME_SCOPED_THREAD_STATE_CHANGE_INL_H_

#include "scoped_thread_state_change.h"

#include "base/casts.h"
#include "jni_env_ext-inl.h"
#include "obj_ptr-inl.h"
#include "runtime.h"
#include "thread-inl.h"

namespace art {

inline ScopedThreadStateChange::ScopedThreadStateChange(Thread* self, ThreadState new_thread_state)
    : self_(self), thread_state_(new_thread_state), expected_has_no_thread_(false) {
  if (UNLIKELY(self_ == nullptr)) {
    // Value chosen arbitrarily and won't be used in the destructor since thread_ == null.
    old_thread_state_ = kTerminated;
    Runtime* runtime = Runtime::Current();
    CHECK(runtime == nullptr || !runtime->IsStarted() || runtime->IsShuttingDown(self_));
  } else {
    DCHECK_EQ(self, Thread::Current());
    // Read state without locks, ok as state is effectively thread local and we're not interested
    // in the suspend count (this will be handled in the runnable transitions).
    old_thread_state_ = self->GetState();
    if (old_thread_state_ != new_thread_state) {
      if (new_thread_state == kRunnable) {
        self_->TransitionFromSuspendedToRunnable();
      } else if (old_thread_state_ == kRunnable) {
        self_->TransitionFromRunnableToSuspended(new_thread_state);
      } else {
        // A suspended transition to another effectively suspended transition, ok to use Unsafe.
        self_->SetState(new_thread_state);
      }
    }
  }
}

inline ScopedThreadStateChange::~ScopedThreadStateChange() {
  if (UNLIKELY(self_ == nullptr)) {
    if (!expected_has_no_thread_) {
      Runtime* runtime = Runtime::Current();
      bool shutting_down = (runtime == nullptr) || runtime->IsShuttingDown(nullptr);
      CHECK(shutting_down);
    }
  } else {
    if (old_thread_state_ != thread_state_) {
      if (old_thread_state_ == kRunnable) {
        self_->TransitionFromSuspendedToRunnable();
      } else if (thread_state_ == kRunnable) {
        self_->TransitionFromRunnableToSuspended(old_thread_state_);
      } else {
        // A suspended transition to another effectively suspended transition, ok to use Unsafe.
        self_->SetState(old_thread_state_);
      }
    }
  }
}

template<typename T>
inline T ScopedObjectAccessAlreadyRunnable::AddLocalReference(ObjPtr<mirror::Object> obj) const {
  Locks::mutator_lock_->AssertSharedHeld(Self());
  if (kIsDebugBuild) {
    CHECK(IsRunnable());  // Don't work with raw objects in non-runnable states.
    DCheckObjIsNotClearedJniWeakGlobal(obj);
  }
  return obj == nullptr ? nullptr : Env()->AddLocalReference<T>(obj);
}

template<typename T>
inline ObjPtr<T> ScopedObjectAccessAlreadyRunnable::Decode(jobject obj) const {
  Locks::mutator_lock_->AssertSharedHeld(Self());
  DCHECK(IsRunnable());  // Don't work with raw objects in non-runnable states.
  return ObjPtr<T>::DownCast(Self()->DecodeJObject(obj));
}

inline bool ScopedObjectAccessAlreadyRunnable::IsRunnable() const {
  return self_->GetState() == kRunnable;
}

inline ScopedObjectAccessAlreadyRunnable::ScopedObjectAccessAlreadyRunnable(JNIEnv* env)
    : self_(ThreadForEnv(env)), env_(down_cast<JNIEnvExt*>(env)), vm_(env_->vm) {}

inline ScopedObjectAccessAlreadyRunnable::ScopedObjectAccessAlreadyRunnable(Thread* self)
    : self_(self),
      env_(down_cast<JNIEnvExt*>(self->GetJniEnv())),
      vm_(env_ != nullptr ? env_->vm : nullptr) {}

inline ScopedObjectAccessUnchecked::ScopedObjectAccessUnchecked(JNIEnv* env)
    : ScopedObjectAccessAlreadyRunnable(env), tsc_(Self(), kRunnable) {
  Self()->VerifyStack();
  Locks::mutator_lock_->AssertSharedHeld(Self());
}

inline ScopedObjectAccessUnchecked::ScopedObjectAccessUnchecked(Thread* self)
    : ScopedObjectAccessAlreadyRunnable(self), tsc_(self, kRunnable) {
  Self()->VerifyStack();
  Locks::mutator_lock_->AssertSharedHeld(Self());
}

inline ScopedObjectAccess::ScopedObjectAccess(JNIEnv* env) : ScopedObjectAccessUnchecked(env) {}
inline ScopedObjectAccess::ScopedObjectAccess(Thread* self) : ScopedObjectAccessUnchecked(self) {}
inline ScopedObjectAccess::~ScopedObjectAccess() {}

inline ScopedThreadSuspension::ScopedThreadSuspension(Thread* self, ThreadState suspended_state)
    : self_(self), suspended_state_(suspended_state) {
  DCHECK(self_ != nullptr);
  self_->TransitionFromRunnableToSuspended(suspended_state);
}

inline ScopedThreadSuspension::~ScopedThreadSuspension() {
  DCHECK_EQ(self_->GetState(), suspended_state_);
  self_->TransitionFromSuspendedToRunnable();
}

}  // namespace art

#endif  // ART_RUNTIME_SCOPED_THREAD_STATE_CHANGE_INL_H_
