/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_CLASS_TABLE_INL_H_
#define ART_RUNTIME_CLASS_TABLE_INL_H_

#include "class_table.h"

namespace art {

template<class Visitor>
void ClassTable::VisitRoots(Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (GcRoot<mirror::Class>& root : class_set) {
      visitor.VisitRoot(root.AddressWithoutBarrier());
    }
  }
  for (GcRoot<mirror::Object>& root : strong_roots_) {
    visitor.VisitRoot(root.AddressWithoutBarrier());
  }
}

template<class Visitor>
void ClassTable::VisitRoots(const Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (GcRoot<mirror::Class>& root : class_set) {
      visitor.VisitRoot(root.AddressWithoutBarrier());
    }
  }
  for (GcRoot<mirror::Object>& root : strong_roots_) {
    visitor.VisitRoot(root.AddressWithoutBarrier());
  }
}

template <typename Visitor>
bool ClassTable::Visit(Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (GcRoot<mirror::Class>& root : class_set) {
      if (!visitor(root.Read())) {
        return false;
      }
    }
  }
  return true;
}


}  // namespace art

#endif  // ART_RUNTIME_CLASS_TABLE_INL_H_
