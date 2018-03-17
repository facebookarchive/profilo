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

#ifndef ART_RUNTIME_MIRROR_METHOD_HANDLE_IMPL_H_
#define ART_RUNTIME_MIRROR_METHOD_HANDLE_IMPL_H_

#include "art_field.h"
#include "art_method.h"
#include "class.h"
#include "gc_root.h"
#include "object.h"
#include "method_type.h"

namespace art {

struct MethodHandleOffsets;
struct MethodHandleImplOffsets;

namespace mirror {

// C++ mirror of java.lang.invoke.MethodHandle
class MANAGED MethodHandle : public Object {
 public:
  // Defines the behaviour of a given method handle. The behaviour
  // of a handle of a given kind is identical to the dex bytecode behaviour
  // of the equivalent instruction.
  //
  // NOTE: These must be kept in sync with the constants defined in
  // java.lang.invoke.MethodHandle.
  enum Kind {
    kInvokeVirtual = 0,
    kInvokeSuper,
    kInvokeDirect,
    kInvokeStatic,
    kInvokeInterface,
    kInvokeTransform,
    kInvokeCallSiteTransform,
    kInstanceGet,
    kInstancePut,
    kStaticGet,
    kStaticPut,
    kLastValidKind = kStaticPut,
    kFirstAccessorKind = kInstanceGet,
    kLastAccessorKind = kStaticPut,
    kLastInvokeKind = kInvokeCallSiteTransform
  };

  Kind GetHandleKind() REQUIRES_SHARED(Locks::mutator_lock_) {
    const int32_t handle_kind = GetField32(OFFSET_OF_OBJECT_MEMBER(MethodHandle, handle_kind_));
    DCHECK(handle_kind >= 0 &&
           handle_kind <= static_cast<int32_t>(Kind::kLastValidKind));
    return static_cast<Kind>(handle_kind);
  }

  ALWAYS_INLINE mirror::MethodType* GetMethodType() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE mirror::MethodType* GetNominalType() REQUIRES_SHARED(Locks::mutator_lock_);

  ArtField* GetTargetField() REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<ArtField*>(
        GetField64(OFFSET_OF_OBJECT_MEMBER(MethodHandle, art_field_or_method_)));
  }

  ArtMethod* GetTargetMethod() REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<ArtMethod*>(
        GetField64(OFFSET_OF_OBJECT_MEMBER(MethodHandle, art_field_or_method_)));
  }

  ALWAYS_INLINE ObjPtr<mirror::Class> GetTargetClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  void Initialize(uintptr_t art_field_or_method, Kind kind, Handle<MethodType> method_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  HeapReference<mirror::MethodHandle> cached_spread_invoker_;
  HeapReference<mirror::MethodType> nominal_type_;
  HeapReference<mirror::MethodType> method_type_;
  uint32_t handle_kind_;
  uint64_t art_field_or_method_;

 private:
  static MemberOffset CachedSpreadInvokerOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandle, cached_spread_invoker_));
  }
  static MemberOffset NominalTypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandle, nominal_type_));
  }
  static MemberOffset MethodTypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandle, method_type_));
  }
  static MemberOffset ArtFieldOrMethodOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandle, art_field_or_method_));
  }
  static MemberOffset HandleKindOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandle, handle_kind_));
  }

  friend struct art::MethodHandleOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(MethodHandle);
};

// C++ mirror of java.lang.invoke.MethodHandleImpl
class MANAGED MethodHandleImpl : public MethodHandle {
 public:
  static mirror::MethodHandleImpl* Create(Thread* const self,
                                          uintptr_t art_field_or_method,
                                          MethodHandle::Kind kind,
                                          Handle<MethodType> method_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset InfoOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodHandleImpl, info_));
  }

  HeapReference<mirror::Object> info_;  // Unused by the runtime.
  static GcRoot<mirror::Class> static_class_;  // java.lang.invoke.MethodHandleImpl.class

  friend struct art::MethodHandleImplOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(MethodHandleImpl);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_HANDLE_IMPL_H_
