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

#ifndef ART_RUNTIME_MIRROR_REFERENCE_INL_H_
#define ART_RUNTIME_MIRROR_REFERENCE_INL_H_

#include "reference.h"

#include "gc_root-inl.h"
#include "obj_ptr-inl.h"
#include "runtime.h"

namespace art {
namespace mirror {

inline uint32_t Reference::ClassSize(PointerSize pointer_size) {
  uint32_t vtable_entries = Object::kVTableLength + 4;
  return Class::ComputeClassSize(false, vtable_entries, 2, 0, 0, 0, 0, pointer_size);
}

template<bool kTransactionActive>
inline void Reference::SetReferent(ObjPtr<Object> referent) {
  SetFieldObjectVolatile<kTransactionActive>(ReferentOffset(), referent);
}

inline void Reference::SetPendingNext(ObjPtr<Reference> pending_next) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetFieldObject<true>(PendingNextOffset(), pending_next);
  } else {
    SetFieldObject<false>(PendingNextOffset(), pending_next);
  }
}

template<bool kTransactionActive>
inline void FinalizerReference::SetZombie(ObjPtr<Object> zombie) {
  return SetFieldObjectVolatile<kTransactionActive>(ZombieOffset(), zombie);
}

template<ReadBarrierOption kReadBarrierOption>
inline Class* Reference::GetJavaLangRefReference() {
  DCHECK(!java_lang_ref_Reference_.IsNull());
  return java_lang_ref_Reference_.Read<kReadBarrierOption>();
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_REFERENCE_INL_H_
