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

#ifndef ART_RUNTIME_OBJ_PTR_INL_H_
#define ART_RUNTIME_OBJ_PTR_INL_H_

#include "base/bit_utils.h"
#include "obj_ptr.h"
#include "thread-current-inl.h"

namespace art {

template<class MirrorType>
inline bool ObjPtr<MirrorType>::IsValid() const {
  if (!kObjPtrPoisoning || IsNull()) {
    return true;
  }
  return GetCookie() == TrimCookie(Thread::Current()->GetPoisonObjectCookie());
}

template<class MirrorType>
inline void ObjPtr<MirrorType>::AssertValid() const {
  if (kObjPtrPoisoning) {
    CHECK(IsValid()) << "Stale object pointer " << PtrUnchecked() << " , expected cookie "
        << TrimCookie(Thread::Current()->GetPoisonObjectCookie()) << " but got " << GetCookie();
  }
}

template<class MirrorType>
inline uintptr_t ObjPtr<MirrorType>::Encode(MirrorType* ptr) {
  uintptr_t ref = reinterpret_cast<uintptr_t>(ptr);
  DCHECK_ALIGNED(ref, kObjectAlignment);
  if (kObjPtrPoisoning && ref != 0) {
    DCHECK_LE(ref, 0xFFFFFFFFU);
    ref >>= kObjectAlignmentShift;
    // Put cookie in high bits.
    Thread* self = Thread::Current();
    DCHECK(self != nullptr);
    ref |= self->GetPoisonObjectCookie() << kCookieShift;
  }
  return ref;
}

template<class MirrorType>
inline std::ostream& operator<<(std::ostream& os, ObjPtr<MirrorType> ptr) {
  // May be used for dumping bad pointers, do not use the checked version.
  return os << ptr.PtrUnchecked();
}

}  // namespace art

#endif  // ART_RUNTIME_OBJ_PTR_INL_H_
