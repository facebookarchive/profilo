/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_RUNTIME_INL_H_
#define ART_RUNTIME_RUNTIME_INL_H_

#include "runtime.h"

#include "art_method.h"
#include "class_linker.h"
#include "gc_root-inl.h"
#include "obj_ptr-inl.h"
#include "read_barrier-inl.h"

namespace art {

inline bool Runtime::IsClearedJniWeakGlobal(ObjPtr<mirror::Object> obj) {
  return obj == GetClearedJniWeakGlobal();
}

inline mirror::Object* Runtime::GetClearedJniWeakGlobal() {
  mirror::Object* obj = sentinel_.Read();
  DCHECK(obj != nullptr);
  return obj;
}

inline QuickMethodFrameInfo Runtime::GetRuntimeMethodFrameInfo(ArtMethod* method) {
  DCHECK(method != nullptr);
  // Cannot be imt-conflict-method or resolution-method.
  DCHECK_NE(method, GetImtConflictMethod());
  DCHECK_NE(method, GetResolutionMethod());
  // Don't use GetCalleeSaveMethod(), some tests don't set all callee save methods.
  if (method == GetCalleeSaveMethodUnchecked(Runtime::kSaveRefsAndArgs)) {
    return GetCalleeSaveMethodFrameInfo(Runtime::kSaveRefsAndArgs);
  } else if (method == GetCalleeSaveMethodUnchecked(Runtime::kSaveAllCalleeSaves)) {
    return GetCalleeSaveMethodFrameInfo(Runtime::kSaveAllCalleeSaves);
  } else if (method == GetCalleeSaveMethodUnchecked(Runtime::kSaveRefsOnly)) {
    return GetCalleeSaveMethodFrameInfo(Runtime::kSaveRefsOnly);
  } else {
    DCHECK_EQ(method, GetCalleeSaveMethodUnchecked(Runtime::kSaveEverything));
    return GetCalleeSaveMethodFrameInfo(Runtime::kSaveEverything);
  }
}

inline ArtMethod* Runtime::GetResolutionMethod() {
  CHECK(HasResolutionMethod());
  return resolution_method_;
}

inline ArtMethod* Runtime::GetImtConflictMethod() {
  CHECK(HasImtConflictMethod());
  return imt_conflict_method_;
}

inline ArtMethod* Runtime::GetImtUnimplementedMethod() {
  CHECK(imt_unimplemented_method_ != nullptr);
  return imt_unimplemented_method_;
}

inline ArtMethod* Runtime::GetCalleeSaveMethod(CalleeSaveType type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(HasCalleeSaveMethod(type));
  return GetCalleeSaveMethodUnchecked(type);
}

inline ArtMethod* Runtime::GetCalleeSaveMethodUnchecked(CalleeSaveType type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return reinterpret_cast<ArtMethod*>(callee_save_methods_[type]);
}

}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_INL_H_
