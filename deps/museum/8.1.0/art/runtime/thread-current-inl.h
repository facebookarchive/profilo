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

#ifndef ART_RUNTIME_THREAD_CURRENT_INL_H_
#define ART_RUNTIME_THREAD_CURRENT_INL_H_

#include "thread.h"

#ifdef ART_TARGET_ANDROID
#include <bionic_tls.h>  // Access to our own TLS slot.
#endif

#include <pthread.h>

namespace art {

inline Thread* Thread::Current() {
  // We rely on Thread::Current returning null for a detached thread, so it's not obvious
  // that we can replace this with a direct %fs access on x86.
  if (!is_started_) {
    return nullptr;
  } else {
#ifdef ART_TARGET_ANDROID
    void* thread = __get_tls()[TLS_SLOT_ART_THREAD_SELF];
#else
    void* thread = pthread_getspecific(Thread::pthread_key_self_);
#endif
    return reinterpret_cast<Thread*>(thread);
  }
}

}  // namespace art

#endif  // ART_RUNTIME_THREAD_CURRENT_INL_H_
