/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_NATIVE_SCOPED_FAST_NATIVE_OBJECT_ACCESS_H_
#define ART_RUNTIME_NATIVE_SCOPED_FAST_NATIVE_OBJECT_ACCESS_H_

#include "mirror/art_method.h"
#include "scoped_thread_state_change.h"

namespace art {

// Variant of ScopedObjectAccess that does no runnable transitions. Should only be used by "fast"
// JNI methods.
class ScopedFastNativeObjectAccess : public ScopedObjectAccessAlreadyRunnable {
 public:
  explicit ScopedFastNativeObjectAccess(JNIEnv* env)
    LOCKS_EXCLUDED(Locks::thread_suspend_count_lock_)
    SHARED_LOCK_FUNCTION(Locks::mutator_lock_) ALWAYS_INLINE
     : ScopedObjectAccessAlreadyRunnable(env) {
    Locks::mutator_lock_->AssertSharedHeld(Self());
    DCHECK(Self()->GetManagedStack()->GetTopQuickFrame()->AsMirrorPtr()->IsFastNative());
    // Don't work with raw objects in non-runnable states.
    DCHECK_EQ(Self()->GetState(), kRunnable);
  }

  ~ScopedFastNativeObjectAccess() UNLOCK_FUNCTION(Locks::mutator_lock_) ALWAYS_INLINE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedFastNativeObjectAccess);
};

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_SCOPED_FAST_NATIVE_OBJECT_ACCESS_H_
