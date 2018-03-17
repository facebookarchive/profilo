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

#ifndef ART_RUNTIME_MIRROR_CALL_SITE_H_
#define ART_RUNTIME_MIRROR_CALL_SITE_H_

#include <museum/8.0.0/art/runtime/mirror/method_handle_impl.h>
#include <museum/8.0.0/art/runtime/utils.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

struct CallSiteOffsets;

namespace mirror {

// C++ mirror of java.lang.invoke.CallSite
class MANAGED CallSite : public Object {
 public:
  static mirror::CallSite* Create(Thread* const self,
                                  Handle<MethodHandle> method_handle)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  MethodHandle* GetTarget() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<MethodHandle>(TargetOffset());
  }

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static inline MemberOffset TargetOffset() {
    return MemberOffset(OFFSETOF_MEMBER(CallSite, target_));
  }

  HeapReference<mirror::MethodHandle> target_;

  static GcRoot<mirror::Class> static_class_;  // java.lang.invoke.CallSite.class

  friend struct facebook::museum::MUSEUM_VERSION::art::CallSiteOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallSite);
};

}  // namespace mirror
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_MIRROR_CALL_SITE_H_
