/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_FIELD_INL_H_
#define ART_RUNTIME_MIRROR_FIELD_INL_H_

#include "field.h"

#include "art_field-inl.h"
#include "mirror/dex_cache-inl.h"
#include "runtime-inl.h"

namespace art {

namespace mirror {

template <bool kTransactionActive>
inline mirror::Field* Field::CreateFromArtField(Thread* self, ArtField* field,
                                                bool force_resolve) {
  StackHandleScope<2> hs(self);
  // Try to resolve type before allocating since this is a thread suspension point.
  Handle<mirror::Class> type = hs.NewHandle(field->GetType<true>());

  if (type.Get() == nullptr) {
    if (force_resolve) {
      if (kIsDebugBuild) {
        self->AssertPendingException();
      }
      return nullptr;
    } else {
      // Can't resolve, clear the exception if it isn't OOME and continue with a null type.
      mirror::Throwable* exception = self->GetException();
      if (exception->GetClass()->DescriptorEquals("Ljava/lang/OutOfMemoryError;")) {
        return nullptr;
      }
      self->ClearException();
    }
  }
  auto ret = hs.NewHandle(static_cast<Field*>(StaticClass()->AllocObject(self)));
  if (UNLIKELY(ret.Get() == nullptr)) {
    self->AssertPendingOOMException();
    return nullptr;
  }
  const auto pointer_size = kTransactionActive ?
      Runtime::Current()->GetClassLinker()->GetImagePointerSize() : sizeof(void*);
  auto dex_field_index = field->GetDexFieldIndex();
  auto* resolved_field = field->GetDexCache()->GetResolvedField(dex_field_index, pointer_size);
  if (field->GetDeclaringClass()->IsProxyClass()) {
    DCHECK(field->IsStatic());
    DCHECK_LT(dex_field_index, 2U);
    // The two static fields (interfaces, throws) of all proxy classes
    // share the same dex file indices 0 and 1. So, we can't resolve
    // them in the dex cache.
  } else {
    if (resolved_field != nullptr) {
      DCHECK_EQ(resolved_field, field);
    } else {
      // We rely on the field being resolved so that we can back to the ArtField
      // (i.e. FromReflectedMethod).
      field->GetDexCache()->SetResolvedField(dex_field_index, field, pointer_size);
    }
  }
  ret->SetType<kTransactionActive>(type.Get());
  ret->SetDeclaringClass<kTransactionActive>(field->GetDeclaringClass());
  ret->SetAccessFlags<kTransactionActive>(field->GetAccessFlags());
  ret->SetDexFieldIndex<kTransactionActive>(dex_field_index);
  ret->SetOffset<kTransactionActive>(field->GetOffset().Int32Value());
  return ret.Get();
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_FIELD_INL_H_
