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

#ifndef ART_RUNTIME_MIRROR_CLASS_REFVISITOR_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_REFVISITOR_INL_H_

#include "class-inl.h"

#include "art_field-inl.h"
#include "class_ext-inl.h"

namespace art {
namespace mirror {

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void Class::VisitReferences(ObjPtr<Class> klass, const Visitor& visitor) {
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass.Ptr(), visitor);
  // Right after a class is allocated, but not yet loaded
  // (kStatusNotReady, see ClassLinker::LoadClass()), GC may find it
  // and scan it. IsTemp() may call Class::GetAccessFlags() but may
  // fail in the DCHECK in Class::GetAccessFlags() because the class
  // status is kStatusNotReady. To avoid it, rely on IsResolved()
  // only. This is fine because a temp class never goes into the
  // kStatusResolved state.
  if (IsResolved<kVerifyFlags>()) {
    // Temp classes don't ever populate imt/vtable or static fields and they are not even
    // allocated with the right size for those. Also, unresolved classes don't have fields
    // linked yet.
    VisitStaticFieldsReferences<kVerifyFlags, kReadBarrierOption>(this, visitor);
  }
  if (kVisitNativeRoots) {
    // Since this class is reachable, we must also visit the associated roots when we scan it.
    VisitNativeRoots<kReadBarrierOption>(
        visitor, Runtime::Current()->GetClassLinker()->GetImagePointerSize());
  }
}

template<ReadBarrierOption kReadBarrierOption, class Visitor>
void Class::VisitNativeRoots(Visitor& visitor, PointerSize pointer_size) {
  for (ArtField& field : GetSFieldsUnchecked()) {
    // Visit roots first in case the declaring class gets moved.
    field.VisitRoots(visitor);
    if (kIsDebugBuild && IsResolved()) {
      CHECK_EQ(field.GetDeclaringClass<kReadBarrierOption>(), this) << GetStatus();
    }
  }
  for (ArtField& field : GetIFieldsUnchecked()) {
    // Visit roots first in case the declaring class gets moved.
    field.VisitRoots(visitor);
    if (kIsDebugBuild && IsResolved()) {
      CHECK_EQ(field.GetDeclaringClass<kReadBarrierOption>(), this) << GetStatus();
    }
  }
  for (ArtMethod& method : GetMethods(pointer_size)) {
    method.VisitRoots<kReadBarrierOption>(visitor, pointer_size);
  }
  ObjPtr<ClassExt> ext(GetExtData<kDefaultVerifyFlags, kReadBarrierOption>());
  if (!ext.IsNull()) {
    ext->VisitNativeRoots<kReadBarrierOption, Visitor>(visitor, pointer_size);
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_REFVISITOR_INL_H_
