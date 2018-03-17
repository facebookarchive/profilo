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

#ifndef ART_RUNTIME_SCOPED_THREAD_STATE_CHANGE_H_
#define ART_RUNTIME_SCOPED_THREAD_STATE_CHANGE_H_

#include "jni.h"

#include "base/macros.h"
#include "base/mutex.h"
#include "base/value_object.h"
#include "thread_state.h"

namespace art {

class JavaVMExt;
struct JNIEnvExt;
template<class MirrorType> class ObjPtr;
class Thread;

namespace mirror {
class Object;
}  // namespace mirror

// Scoped change into and out of a particular state. Handles Runnable transitions that require
// more complicated suspension checking. The subclasses ScopedObjectAccessUnchecked and
// ScopedObjectAccess are used to handle the change into Runnable to Get direct access to objects,
// the unchecked variant doesn't aid annotalysis.
class ScopedThreadStateChange : public ValueObject {
 public:
  ALWAYS_INLINE ScopedThreadStateChange(Thread* self, ThreadState new_thread_state)
      REQUIRES(!Locks::thread_suspend_count_lock_);

  ALWAYS_INLINE ~ScopedThreadStateChange() REQUIRES(!Locks::thread_suspend_count_lock_);

  ALWAYS_INLINE Thread* Self() const {
    return self_;
  }

 protected:
  // Constructor used by ScopedJniThreadState for an unattached thread that has access to the VM*.
  ScopedThreadStateChange() {}

  Thread* const self_ = nullptr;
  const ThreadState thread_state_ = kTerminated;

 private:
  ThreadState old_thread_state_ = kTerminated;
  const bool expected_has_no_thread_ = true;

  friend class ScopedObjectAccessUnchecked;
  DISALLOW_COPY_AND_ASSIGN(ScopedThreadStateChange);
};

// Assumes we are already runnable.
class ScopedObjectAccessAlreadyRunnable : public ValueObject {
 public:
  Thread* Self() const {
    return self_;
  }

  JNIEnvExt* Env() const {
    return env_;
  }

  JavaVMExt* Vm() const {
    return vm_;
  }

  bool ForceCopy() const;

  /*
   * Add a local reference for an object to the indirect reference table associated with the
   * current stack frame.  When the native function returns, the reference will be discarded.
   *
   * We need to allow the same reference to be added multiple times, and cope with nullptr.
   *
   * This will be called on otherwise unreferenced objects. We cannot do GC allocations here, and
   * it's best if we don't grab a mutex.
   */
  template<typename T>
  T AddLocalReference(ObjPtr<mirror::Object> obj) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<typename T>
  ObjPtr<T> Decode(jobject obj) const REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE bool IsRunnable() const;

 protected:
  ALWAYS_INLINE explicit ScopedObjectAccessAlreadyRunnable(JNIEnv* env)
      REQUIRES(!Locks::thread_suspend_count_lock_);

  ALWAYS_INLINE explicit ScopedObjectAccessAlreadyRunnable(Thread* self)
      REQUIRES(!Locks::thread_suspend_count_lock_);

  // Used when we want a scoped JNI thread state but have no thread/JNIEnv. Consequently doesn't
  // change into Runnable or acquire a share on the mutator_lock_.
  // Note: The reinterpret_cast is backed by a static_assert in the cc file. Avoid a down_cast,
  //       as it prevents forward declaration of JavaVMExt.
  explicit ScopedObjectAccessAlreadyRunnable(JavaVM* vm)
      : self_(nullptr), env_(nullptr), vm_(reinterpret_cast<JavaVMExt*>(vm)) {}

  // Here purely to force inlining.
  ALWAYS_INLINE ~ScopedObjectAccessAlreadyRunnable() {}

  static void DCheckObjIsNotClearedJniWeakGlobal(ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Self thread, can be null.
  Thread* const self_;
  // The full JNIEnv.
  JNIEnvExt* const env_;
  // The full JavaVM.
  JavaVMExt* const vm_;
};

// Entry/exit processing for transitions from Native to Runnable (ie within JNI functions).
//
// This class performs the necessary thread state switching to and from Runnable and lets us
// amortize the cost of working out the current thread. Additionally it lets us check (and repair)
// apps that are using a JNIEnv on the wrong thread. The class also decodes and encodes Objects
// into jobjects via methods of this class. Performing this here enforces the Runnable thread state
// for use of Object, thereby inhibiting the Object being modified by GC whilst native or VM code
// is also manipulating the Object.
//
// The destructor transitions back to the previous thread state, typically Native. In this state
// GC and thread suspension may occur.
//
// For annotalysis the subclass ScopedObjectAccess (below) makes it explicit that a shared of
// the mutator_lock_ will be acquired on construction.
class ScopedObjectAccessUnchecked : public ScopedObjectAccessAlreadyRunnable {
 public:
  ALWAYS_INLINE explicit ScopedObjectAccessUnchecked(JNIEnv* env)
      REQUIRES(!Locks::thread_suspend_count_lock_);

  ALWAYS_INLINE explicit ScopedObjectAccessUnchecked(Thread* self)
      REQUIRES(!Locks::thread_suspend_count_lock_);

  ALWAYS_INLINE ~ScopedObjectAccessUnchecked() REQUIRES(!Locks::thread_suspend_count_lock_) {}

  // Used when we want a scoped JNI thread state but have no thread/JNIEnv. Consequently doesn't
  // change into Runnable or acquire a share on the mutator_lock_.
  explicit ScopedObjectAccessUnchecked(JavaVM* vm) ALWAYS_INLINE
      : ScopedObjectAccessAlreadyRunnable(vm), tsc_() {}

 private:
  // The scoped thread state change makes sure that we are runnable and restores the thread state
  // in the destructor.
  const ScopedThreadStateChange tsc_;

  DISALLOW_COPY_AND_ASSIGN(ScopedObjectAccessUnchecked);
};

// Annotalysis helping variant of the above.
class ScopedObjectAccess : public ScopedObjectAccessUnchecked {
 public:
  ALWAYS_INLINE explicit ScopedObjectAccess(JNIEnv* env)
      REQUIRES(!Locks::thread_suspend_count_lock_)
      SHARED_LOCK_FUNCTION(Locks::mutator_lock_);

  ALWAYS_INLINE explicit ScopedObjectAccess(Thread* self)
      REQUIRES(!Locks::thread_suspend_count_lock_)
      SHARED_LOCK_FUNCTION(Locks::mutator_lock_);

  // Base class will release share of lock. Invoked after this destructor.
  ~ScopedObjectAccess() UNLOCK_FUNCTION(Locks::mutator_lock_) ALWAYS_INLINE;

 private:
  // TODO: remove this constructor. It is used by check JNI's ScopedCheck to make it believe that
  //       routines operating with just a VM are sound, they are not, but when you have just a VM
  //       you cannot call the unsound routines.
  explicit ScopedObjectAccess(JavaVM* vm) SHARED_LOCK_FUNCTION(Locks::mutator_lock_)
      : ScopedObjectAccessUnchecked(vm) {}

  friend class ScopedCheck;
  DISALLOW_COPY_AND_ASSIGN(ScopedObjectAccess);
};

// Annotalysis helper for going to a suspended state from runnable.
class ScopedThreadSuspension : public ValueObject {
 public:
  ALWAYS_INLINE explicit ScopedThreadSuspension(Thread* self, ThreadState suspended_state)
      REQUIRES(!Locks::thread_suspend_count_lock_, !Roles::uninterruptible_)
      UNLOCK_FUNCTION(Locks::mutator_lock_);

  ALWAYS_INLINE ~ScopedThreadSuspension() SHARED_LOCK_FUNCTION(Locks::mutator_lock_);

 private:
  Thread* const self_;
  const ThreadState suspended_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedThreadSuspension);
};


}  // namespace art

#endif  // ART_RUNTIME_SCOPED_THREAD_STATE_CHANGE_H_
