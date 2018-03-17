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

#ifndef ART_RUNTIME_MIRROR_METHOD_TYPE_H_
#define ART_RUNTIME_MIRROR_METHOD_TYPE_H_

#include "object.h"
#include "string.h"
#include "mirror/object_array.h"
#include "utils.h"

namespace art {

struct MethodTypeOffsets;

namespace mirror {

// C++ mirror of java.lang.invoke.MethodType
class MANAGED MethodType : public Object {
 public:
  static mirror::MethodType* Create(Thread* const self,
                                    Handle<Class> return_type,
                                    Handle<ObjectArray<Class>> param_types)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  ObjectArray<Class>* GetPTypes() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<ObjectArray<Class>>(OFFSET_OF_OBJECT_MEMBER(MethodType, p_types_));
  }

  // Number of virtual registers required to hold the parameters for
  // this method type.
  size_t NumberOfVRegs() REQUIRES_SHARED(Locks::mutator_lock_);

  Class* GetRType() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(MethodType, r_type_));
  }

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true iff. |this| is an exact match for method type |target|, i.e
  // iff. they have the same return types and parameter types.
  bool IsExactMatch(mirror::MethodType* target) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true iff. |this| can be converted to match |target| method type, i.e
  // iff. they have convertible return types and parameter types.
  bool IsConvertible(mirror::MethodType* target) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the pretty descriptor for this method type, suitable for display in
  // exception messages and the like.
  std::string PrettyDescriptor() REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset FormOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, form_));
  }

  static MemberOffset MethodDescriptorOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, method_descriptor_));
  }

  static MemberOffset PTypesOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, p_types_));
  }

  static MemberOffset RTypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, r_type_));
  }

  static MemberOffset WrapAltOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, wrap_alt_));
  }

  HeapReference<mirror::Object> form_;  // Unused in the runtime
  HeapReference<mirror::String> method_descriptor_;  // Unused in the runtime
  HeapReference<ObjectArray<mirror::Class>> p_types_;
  HeapReference<mirror::Class> r_type_;
  HeapReference<mirror::Object> wrap_alt_;  // Unused in the runtime

  static GcRoot<mirror::Class> static_class_;  // java.lang.invoke.MethodType.class

  friend struct art::MethodTypeOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(MethodType);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_TYPE_H_
