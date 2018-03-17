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

#ifndef ART_RUNTIME_MIRROR_ACCESSIBLE_OBJECT_H_
#define ART_RUNTIME_MIRROR_ACCESSIBLE_OBJECT_H_

#include "class.h"
#include "gc_root.h"
#include "object.h"
#include "object_callbacks.h"
#include "read_barrier_option.h"
#include "thread.h"

namespace art {

namespace mirror {

// C++ mirror of java.lang.reflect.AccessibleObject
class MANAGED AccessibleObject : public Object {
 public:
  static MemberOffset FlagOffset() {
    return OFFSET_OF_OBJECT_MEMBER(AccessibleObject, flag_);
  }

  template<bool kTransactionActive>
  void SetAccessible(bool value) SHARED_REQUIRES(Locks::mutator_lock_) {
    UNUSED(padding_);
    return SetFieldBoolean<kTransactionActive>(FlagOffset(), value ? 1u : 0u);
  }

  bool IsAccessible() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldBoolean(FlagOffset());
  }

 private:
  uint8_t flag_;
  // Padding required for now since "packed" will cause reflect.Field fields to not be aligned
  // otherwise.
  uint8_t padding_[3];

  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessibleObject);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ACCESSIBLE_OBJECT_H_
