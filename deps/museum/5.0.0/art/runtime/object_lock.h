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

#ifndef ART_RUNTIME_OBJECT_LOCK_H_
#define ART_RUNTIME_OBJECT_LOCK_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "handle.h"

namespace art {

class Thread;

template <typename T>
class ObjectLock {
 public:
  ObjectLock(Thread* self, Handle<T> object) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ~ObjectLock() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void WaitIgnoringInterrupts() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Notify() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void NotifyAll() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  Thread* const self_;
  Handle<T> const obj_;

  DISALLOW_COPY_AND_ASSIGN(ObjectLock);
};

}  // namespace art

#endif  // ART_RUNTIME_OBJECT_LOCK_H_
