/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_METHOD_HANDLES_LOOKUP_H_
#define ART_RUNTIME_MIRROR_METHOD_HANDLES_LOOKUP_H_

#include "obj_ptr.h"
#include "gc_root.h"
#include "object.h"
#include "handle.h"
#include "utils.h"

namespace art {

struct MethodHandlesLookupOffsets;
class RootVisitor;

namespace mirror {

// C++ mirror of java.lang.invoke.MethodHandles.Lookup
class MANAGED MethodHandlesLookup : public Object {
 public:
  static mirror::MethodHandlesLookup* Create(Thread* const self,
                                             Handle<Class> lookup_class)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset AllowedModesOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandlesLookup, allowed_modes_));
  }

  static MemberOffset LookupClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandlesLookup, lookup_class_));
  }

  HeapReference<mirror::Class> lookup_class_;

  int32_t allowed_modes_;

  static GcRoot<mirror::Class> static_class_;  // java.lang.invoke.MethodHandles.Lookup.class

  friend struct art::MethodHandlesLookupOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(MethodHandlesLookup);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_HANDLES_LOOKUP_H_
