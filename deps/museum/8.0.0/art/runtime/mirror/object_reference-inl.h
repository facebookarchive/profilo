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

#ifndef ART_RUNTIME_MIRROR_OBJECT_REFERENCE_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_REFERENCE_INL_H_

#include "object_reference.h"

#include "obj_ptr-inl.h"

namespace art {
namespace mirror {

template<bool kPoisonReferences, class MirrorType>
void ObjectReference<kPoisonReferences, MirrorType>::Assign(ObjPtr<MirrorType> ptr) {
  Assign(ptr.Ptr());
}

template<class MirrorType>
HeapReference<MirrorType> HeapReference<MirrorType>::FromObjPtr(ObjPtr<MirrorType> ptr) {
  return HeapReference<MirrorType>(ptr.Ptr());
}

template<class MirrorType>
bool HeapReference<MirrorType>::CasWeakRelaxed(MirrorType* expected_ptr, MirrorType* new_ptr) {
  HeapReference<Object> expected_ref(HeapReference<Object>::FromMirrorPtr(expected_ptr));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(new_ptr));
  Atomic<uint32_t>* atomic_reference = reinterpret_cast<Atomic<uint32_t>*>(&this->reference_);
  return atomic_reference->CompareExchangeWeakRelaxed(expected_ref.reference_,
                                                      new_ref.reference_);
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_REFERENCE_INL_H_
