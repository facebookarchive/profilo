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

#ifndef ART_RUNTIME_NATIVE_SCOPED_FAST_NATIVE_OBJECT_ACCESS_INL_H_
#define ART_RUNTIME_NATIVE_SCOPED_FAST_NATIVE_OBJECT_ACCESS_INL_H_

#include "scoped_fast_native_object_access.h"

#include "art_method.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

inline ScopedFastNativeObjectAccess::ScopedFastNativeObjectAccess(JNIEnv* env)
    : ScopedObjectAccessAlreadyRunnable(env) {
  Locks::mutator_lock_->AssertSharedHeld(Self());
  DCHECK((*Self()->GetManagedStack()->GetTopQuickFrame())->IsAnnotatedWithFastNative());
  // Don't work with raw objects in non-runnable states.
  DCHECK_EQ(Self()->GetState(), kRunnable);
}

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_SCOPED_FAST_NATIVE_OBJECT_ACCESS_INL_H_
