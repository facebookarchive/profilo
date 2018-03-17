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

#ifndef ART_RUNTIME_MIRROR_STRING_INL_H_
#define ART_RUNTIME_MIRROR_STRING_INL_H_

#include "array.h"
#include "class.h"
#include "intern_table.h"
#include "runtime.h"
#include "string.h"
#include "thread.h"
#include "utf.h"

namespace art {
namespace mirror {

inline uint32_t String::ClassSize() {
  uint32_t vtable_entries = Object::kVTableLength + 51;
  return Class::ComputeClassSize(true, vtable_entries, 1, 1, 2);
}

inline CharArray* String::GetCharArray() {
  return GetFieldObject<CharArray>(ValueOffset());
}

inline int32_t String::GetLength() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, count_));
  DCHECK(result >= 0 && result <= GetCharArray()->GetLength());
  return result;
}

inline void String::SetArray(CharArray* new_array) {
  // Array is invariant so use non-transactional mode. Also disable check as we may run inside
  // a transaction.
  DCHECK(new_array != NULL);
  SetFieldObject<false, false>(OFFSET_OF_OBJECT_MEMBER(String, array_), new_array);
}

inline String* String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

inline uint16_t String::CharAt(int32_t index) {
  // TODO: do we need this? Equals is the only caller, and could
  // bounds check itself.
  DCHECK_GE(count_, 0);  // ensures the unsigned comparison is safe.
  if (UNLIKELY(static_cast<uint32_t>(index) >= static_cast<uint32_t>(count_))) {
    Thread* self = Thread::Current();
    ThrowLocation throw_location = self->GetCurrentLocationForThrow();
    self->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                             "length=%i; index=%i", count_, index);
    return 0;
  }
  return GetCharArray()->Get(index + GetOffset());
}

inline int32_t String::GetHashCode() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_));
  if (UNLIKELY(result == 0)) {
    result = ComputeHashCode();
  }
  DCHECK(result != 0 || ComputeUtf16Hash(GetCharArray(), GetOffset(), GetLength()) == 0)
      << ToModifiedUtf8() << " " << result;
  return result;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_INL_H_
