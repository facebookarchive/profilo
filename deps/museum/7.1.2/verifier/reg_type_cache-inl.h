/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_INL_H_
#define ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_INL_H_

#include "class_linker.h"
#include "mirror/class-inl.h"
#include "mirror/string.h"
#include "mirror/throwable.h"
#include "reg_type.h"
#include "reg_type_cache.h"

namespace art {
namespace verifier {

inline const art::verifier::RegType& RegTypeCache::GetFromId(uint16_t id) const {
  DCHECK_LT(id, entries_.size());
  const RegType* result = entries_[id];
  DCHECK(result != nullptr);
  return *result;
}

inline const ConstantType& RegTypeCache::FromCat1Const(int32_t value, bool precise) {
  // We only expect 0 to be a precise constant.
  DCHECK(value != 0 || precise);
  if (precise && (value >= kMinSmallConstant) && (value <= kMaxSmallConstant)) {
    return *small_precise_constants_[value - kMinSmallConstant];
  }
  return FromCat1NonSmallConstant(value, precise);
}

inline const ImpreciseConstType& RegTypeCache::ByteConstant() {
  const ConstantType& result = FromCat1Const(std::numeric_limits<jbyte>::min(), false);
  DCHECK(result.IsImpreciseConstant());
  return *down_cast<const ImpreciseConstType*>(&result);
}

inline const ImpreciseConstType& RegTypeCache::CharConstant() {
  int32_t jchar_max = static_cast<int32_t>(std::numeric_limits<jchar>::max());
  const ConstantType& result =  FromCat1Const(jchar_max, false);
  DCHECK(result.IsImpreciseConstant());
  return *down_cast<const ImpreciseConstType*>(&result);
}

inline const ImpreciseConstType& RegTypeCache::ShortConstant() {
  const ConstantType& result =  FromCat1Const(std::numeric_limits<jshort>::min(), false);
  DCHECK(result.IsImpreciseConstant());
  return *down_cast<const ImpreciseConstType*>(&result);
}

inline const ImpreciseConstType& RegTypeCache::IntConstant() {
  const ConstantType& result = FromCat1Const(std::numeric_limits<jint>::max(), false);
  DCHECK(result.IsImpreciseConstant());
  return *down_cast<const ImpreciseConstType*>(&result);
}

inline const ImpreciseConstType& RegTypeCache::PosByteConstant() {
  const ConstantType& result = FromCat1Const(std::numeric_limits<jbyte>::max(), false);
  DCHECK(result.IsImpreciseConstant());
  return *down_cast<const ImpreciseConstType*>(&result);
}

inline const ImpreciseConstType& RegTypeCache::PosShortConstant() {
  const ConstantType& result =  FromCat1Const(std::numeric_limits<jshort>::max(), false);
  DCHECK(result.IsImpreciseConstant());
  return *down_cast<const ImpreciseConstType*>(&result);
}

inline const PreciseReferenceType& RegTypeCache::JavaLangClass() {
  const RegType* result = &FromClass("Ljava/lang/Class;", mirror::Class::GetJavaLangClass(), true);
  DCHECK(result->IsPreciseReference());
  return *down_cast<const PreciseReferenceType*>(result);
}

inline const PreciseReferenceType& RegTypeCache::JavaLangString() {
  // String is final and therefore always precise.
  const RegType* result = &FromClass("Ljava/lang/String;", mirror::String::GetJavaLangString(),
                                     true);
  DCHECK(result->IsPreciseReference());
  return *down_cast<const PreciseReferenceType*>(result);
}

inline const RegType&  RegTypeCache::JavaLangThrowable(bool precise) {
  const RegType* result = &FromClass("Ljava/lang/Throwable;",
                                     mirror::Throwable::GetJavaLangThrowable(), precise);
  if (precise) {
    DCHECK(result->IsPreciseReference());
    return *down_cast<const PreciseReferenceType*>(result);
  } else {
    DCHECK(result->IsReference());
    return *down_cast<const ReferenceType*>(result);
  }
}

inline const RegType& RegTypeCache::JavaLangObject(bool precise) {
  const RegType* result = &FromClass("Ljava/lang/Object;",
                                     mirror::Class::GetJavaLangClass()->GetSuperClass(), precise);
  if (precise) {
    DCHECK(result->IsPreciseReference());
    return *down_cast<const PreciseReferenceType*>(result);
  } else {
    DCHECK(result->IsReference());
    return *down_cast<const ReferenceType*>(result);
  }
}

template <class RegTypeType>
inline RegTypeType& RegTypeCache::AddEntry(RegTypeType* new_entry) {
  DCHECK(new_entry != nullptr);
  entries_.push_back(new_entry);
  if (new_entry->HasClass()) {
    mirror::Class* klass = new_entry->GetClass();
    DCHECK(!klass->IsPrimitive());
    klass_entries_.push_back(std::make_pair(GcRoot<mirror::Class>(klass), new_entry));
  }
  return *new_entry;
}

}  // namespace verifier
}  // namespace art
#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_INL_H_
