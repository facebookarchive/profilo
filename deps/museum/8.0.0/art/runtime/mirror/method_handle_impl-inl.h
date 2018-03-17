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

#ifndef ART_RUNTIME_MIRROR_METHOD_HANDLE_IMPL_INL_H_
#define ART_RUNTIME_MIRROR_METHOD_HANDLE_IMPL_INL_H_

#include "method_handle_impl.h"

#include "art_method-inl.h"
#include "object-inl.h"

namespace art {
namespace mirror {

inline mirror::MethodType* MethodHandle::GetMethodType() {
  return GetFieldObject<mirror::MethodType>(OFFSET_OF_OBJECT_MEMBER(MethodHandle, method_type_));
}

inline mirror::MethodType* MethodHandle::GetNominalType() {
  return GetFieldObject<mirror::MethodType>(OFFSET_OF_OBJECT_MEMBER(MethodHandle, nominal_type_));
}

inline ObjPtr<mirror::Class> MethodHandle::GetTargetClass() {
  Kind kind = GetHandleKind();
  return (kind <= kLastValidKind) ?
      GetTargetMethod()->GetDeclaringClass() : GetTargetField()->GetDeclaringClass();
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_HANDLE_IMPL_INL_H_
