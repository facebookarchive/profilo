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

#ifndef ART_RUNTIME_ART_FIELD_INL_H_
#define ART_RUNTIME_ART_FIELD_INL_H_

#include "art_field.h"

#include "base/logging.h"
#include "class_linker.h"
#include "dex_file-inl.h"
#include "gc_root-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "jvalue.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "primitive.h"
#include "thread-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "well_known_classes.h"

namespace art {

template<ReadBarrierOption kReadBarrierOption>
inline ObjPtr<mirror::Class> ArtField::GetDeclaringClass() {
  GcRootSource gc_root_source(this);
  ObjPtr<mirror::Class> result = declaring_class_.Read<kReadBarrierOption>(&gc_root_source);
  DCHECK(result != nullptr);
  DCHECK(result->IsLoaded() || result->IsErroneous()) << result->GetStatus();
  return result;
}

inline void ArtField::SetDeclaringClass(ObjPtr<mirror::Class> new_declaring_class) {
  declaring_class_ = GcRoot<mirror::Class>(new_declaring_class);
}

inline MemberOffset ArtField::GetOffsetDuringLinking() {
  DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
  return MemberOffset(offset_);
}

inline uint32_t ArtField::Get32(ObjPtr<mirror::Object> object) {
  DCHECK(object != nullptr) << PrettyField();
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    return object->GetField32Volatile(GetOffset());
  }
  return object->GetField32(GetOffset());
}

template<bool kTransactionActive>
inline void ArtField::Set32(ObjPtr<mirror::Object> object, uint32_t new_value) {
  DCHECK(object != nullptr) << PrettyField();
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    object->SetField32Volatile<kTransactionActive>(GetOffset(), new_value);
  } else {
    object->SetField32<kTransactionActive>(GetOffset(), new_value);
  }
}

inline uint64_t ArtField::Get64(ObjPtr<mirror::Object> object) {
  DCHECK(object != nullptr) << PrettyField();
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    return object->GetField64Volatile(GetOffset());
  }
  return object->GetField64(GetOffset());
}

template<bool kTransactionActive>
inline void ArtField::Set64(ObjPtr<mirror::Object> object, uint64_t new_value) {
  DCHECK(object != nullptr) << PrettyField();
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    object->SetField64Volatile<kTransactionActive>(GetOffset(), new_value);
  } else {
    object->SetField64<kTransactionActive>(GetOffset(), new_value);
  }
}

template<class MirrorType>
inline ObjPtr<MirrorType> ArtField::GetObj(ObjPtr<mirror::Object> object) {
  DCHECK(object != nullptr) << PrettyField();
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    return object->GetFieldObjectVolatile<MirrorType>(GetOffset());
  }
  return object->GetFieldObject<MirrorType>(GetOffset());
}

template<bool kTransactionActive>
inline void ArtField::SetObj(ObjPtr<mirror::Object> object, ObjPtr<mirror::Object> new_value) {
  DCHECK(object != nullptr) << PrettyField();
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    object->SetFieldObjectVolatile<kTransactionActive>(GetOffset(), new_value);
  } else {
    object->SetFieldObject<kTransactionActive>(GetOffset(), new_value);
  }
}

#define FIELD_GET(object, type) \
  DCHECK_EQ(Primitive::kPrim ## type, GetTypeAsPrimitiveType()) << PrettyField(); \
  DCHECK((object) != nullptr) << PrettyField(); \
  DCHECK(!IsStatic() || ((object) == GetDeclaringClass()) || !Runtime::Current()->IsStarted()); \
  if (UNLIKELY(IsVolatile())) { \
    return (object)->GetField ## type ## Volatile(GetOffset()); \
  } \
  return (object)->GetField ## type(GetOffset());

#define FIELD_SET(object, type, value) \
  DCHECK((object) != nullptr) << PrettyField(); \
  DCHECK(!IsStatic() || ((object) == GetDeclaringClass()) || !Runtime::Current()->IsStarted()); \
  if (UNLIKELY(IsVolatile())) { \
    (object)->SetField ## type ## Volatile<kTransactionActive>(GetOffset(), value); \
  } else { \
    (object)->SetField ## type<kTransactionActive>(GetOffset(), value); \
  }

inline uint8_t ArtField::GetBoolean(ObjPtr<mirror::Object> object) {
  FIELD_GET(object, Boolean);
}

template<bool kTransactionActive>
inline void ArtField::SetBoolean(ObjPtr<mirror::Object> object, uint8_t z) {
  if (kIsDebugBuild) {
    // For simplicity, this method is being called by the compiler entrypoint for
    // both boolean and byte fields.
    Primitive::Type type = GetTypeAsPrimitiveType();
    DCHECK(type == Primitive::kPrimBoolean || type == Primitive::kPrimByte) << PrettyField();
  }
  FIELD_SET(object, Boolean, z);
}

inline int8_t ArtField::GetByte(ObjPtr<mirror::Object> object) {
  FIELD_GET(object, Byte);
}

template<bool kTransactionActive>
inline void ArtField::SetByte(ObjPtr<mirror::Object> object, int8_t b) {
  DCHECK_EQ(Primitive::kPrimByte, GetTypeAsPrimitiveType()) << PrettyField();
  FIELD_SET(object, Byte, b);
}

inline uint16_t ArtField::GetChar(ObjPtr<mirror::Object> object) {
  FIELD_GET(object, Char);
}

template<bool kTransactionActive>
inline void ArtField::SetChar(ObjPtr<mirror::Object> object, uint16_t c) {
  if (kIsDebugBuild) {
    // For simplicity, this method is being called by the compiler entrypoint for
    // both char and short fields.
    Primitive::Type type = GetTypeAsPrimitiveType();
    DCHECK(type == Primitive::kPrimChar || type == Primitive::kPrimShort) << PrettyField();
  }
  FIELD_SET(object, Char, c);
}

inline int16_t ArtField::GetShort(ObjPtr<mirror::Object> object) {
  FIELD_GET(object, Short);
}

template<bool kTransactionActive>
inline void ArtField::SetShort(ObjPtr<mirror::Object> object, int16_t s) {
  DCHECK_EQ(Primitive::kPrimShort, GetTypeAsPrimitiveType()) << PrettyField();
  FIELD_SET(object, Short, s);
}

#undef FIELD_GET
#undef FIELD_SET

inline int32_t ArtField::GetInt(ObjPtr<mirror::Object> object) {
  if (kIsDebugBuild) {
    // For simplicity, this method is being called by the compiler entrypoint for
    // both int and float fields.
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimInt || type == Primitive::kPrimFloat) << PrettyField();
  }
  return Get32(object);
}

template<bool kTransactionActive>
inline void ArtField::SetInt(ObjPtr<mirror::Object> object, int32_t i) {
  if (kIsDebugBuild) {
    // For simplicity, this method is being called by the compiler entrypoint for
    // both int and float fields.
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimInt || type == Primitive::kPrimFloat) << PrettyField();
  }
  Set32<kTransactionActive>(object, i);
}

inline int64_t ArtField::GetLong(ObjPtr<mirror::Object> object) {
  if (kIsDebugBuild) {
    // For simplicity, this method is being called by the compiler entrypoint for
    // both long and double fields.
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimLong || type == Primitive::kPrimDouble) << PrettyField();
  }
  return Get64(object);
}

template<bool kTransactionActive>
inline void ArtField::SetLong(ObjPtr<mirror::Object> object, int64_t j) {
  if (kIsDebugBuild) {
    // For simplicity, this method is being called by the compiler entrypoint for
    // both long and double fields.
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimLong || type == Primitive::kPrimDouble) << PrettyField();
  }
  Set64<kTransactionActive>(object, j);
}

inline float ArtField::GetFloat(ObjPtr<mirror::Object> object) {
  DCHECK_EQ(Primitive::kPrimFloat, GetTypeAsPrimitiveType()) << PrettyField();
  JValue bits;
  bits.SetI(Get32(object));
  return bits.GetF();
}

template<bool kTransactionActive>
inline void ArtField::SetFloat(ObjPtr<mirror::Object> object, float f) {
  DCHECK_EQ(Primitive::kPrimFloat, GetTypeAsPrimitiveType()) << PrettyField();
  JValue bits;
  bits.SetF(f);
  Set32<kTransactionActive>(object, bits.GetI());
}

inline double ArtField::GetDouble(ObjPtr<mirror::Object> object) {
  DCHECK_EQ(Primitive::kPrimDouble, GetTypeAsPrimitiveType()) << PrettyField();
  JValue bits;
  bits.SetJ(Get64(object));
  return bits.GetD();
}

template<bool kTransactionActive>
inline void ArtField::SetDouble(ObjPtr<mirror::Object> object, double d) {
  DCHECK_EQ(Primitive::kPrimDouble, GetTypeAsPrimitiveType()) << PrettyField();
  JValue bits;
  bits.SetD(d);
  Set64<kTransactionActive>(object, bits.GetJ());
}

inline ObjPtr<mirror::Object> ArtField::GetObject(ObjPtr<mirror::Object> object) {
  DCHECK_EQ(Primitive::kPrimNot, GetTypeAsPrimitiveType()) << PrettyField();
  return GetObj(object);
}

template<bool kTransactionActive>
inline void ArtField::SetObject(ObjPtr<mirror::Object> object, ObjPtr<mirror::Object> l) {
  DCHECK_EQ(Primitive::kPrimNot, GetTypeAsPrimitiveType()) << PrettyField();
  SetObj<kTransactionActive>(object, l);
}

inline const char* ArtField::GetName() REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t field_index = GetDexFieldIndex();
  if (UNLIKELY(GetDeclaringClass()->IsProxyClass())) {
    DCHECK(IsStatic());
    DCHECK_LT(field_index, 2U);
    return field_index == 0 ? "interfaces" : "throws";
  }
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetFieldName(dex_file->GetFieldId(field_index));
}

inline const char* ArtField::GetTypeDescriptor() REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t field_index = GetDexFieldIndex();
  if (UNLIKELY(GetDeclaringClass()->IsProxyClass())) {
    DCHECK(IsStatic());
    DCHECK_LT(field_index, 2U);
    // 0 == Class[] interfaces; 1 == Class[][] throws;
    return field_index == 0 ? "[Ljava/lang/Class;" : "[[Ljava/lang/Class;";
  }
  const DexFile* dex_file = GetDexFile();
  const DexFile::FieldId& field_id = dex_file->GetFieldId(field_index);
  return dex_file->GetFieldTypeDescriptor(field_id);
}

inline Primitive::Type ArtField::GetTypeAsPrimitiveType()
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return Primitive::GetType(GetTypeDescriptor()[0]);
}

inline bool ArtField::IsPrimitiveType() REQUIRES_SHARED(Locks::mutator_lock_) {
  return GetTypeAsPrimitiveType() != Primitive::kPrimNot;
}

template <bool kResolve>
inline ObjPtr<mirror::Class> ArtField::GetType() {
  // TODO: Refactor this function into two functions, ResolveType() and LookupType()
  // so that we can properly annotate it with no-suspension possible / suspension possible.
  const uint32_t field_index = GetDexFieldIndex();
  ObjPtr<mirror::Class> declaring_class = GetDeclaringClass();
  if (UNLIKELY(declaring_class->IsProxyClass())) {
    return ProxyFindSystemClass(GetTypeDescriptor());
  }
  auto* dex_cache = declaring_class->GetDexCache();
  const DexFile* const dex_file = dex_cache->GetDexFile();
  const DexFile::FieldId& field_id = dex_file->GetFieldId(field_index);
  ObjPtr<mirror::Class> type = dex_cache->GetResolvedType(field_id.type_idx_);
  if (UNLIKELY(type == nullptr)) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    if (kResolve) {
      type = class_linker->ResolveType(*dex_file, field_id.type_idx_, declaring_class);
      CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
    } else {
      type = class_linker->LookupResolvedType(
          *dex_file, field_id.type_idx_, dex_cache, declaring_class->GetClassLoader());
      DCHECK(!Thread::Current()->IsExceptionPending());
    }
  }
  return type;
}

inline size_t ArtField::FieldSize() REQUIRES_SHARED(Locks::mutator_lock_) {
  return Primitive::ComponentSize(GetTypeAsPrimitiveType());
}

inline ObjPtr<mirror::DexCache> ArtField::GetDexCache() REQUIRES_SHARED(Locks::mutator_lock_) {
  return GetDeclaringClass()->GetDexCache();
}

inline const DexFile* ArtField::GetDexFile() REQUIRES_SHARED(Locks::mutator_lock_) {
  return GetDexCache()->GetDexFile();
}

inline ObjPtr<mirror::String> ArtField::GetStringName(Thread* self, bool resolve) {
  auto dex_field_index = GetDexFieldIndex();
  CHECK_NE(dex_field_index, DexFile::kDexNoIndex);
  ObjPtr<mirror::DexCache> dex_cache = GetDexCache();
  const auto* dex_file = dex_cache->GetDexFile();
  const auto& field_id = dex_file->GetFieldId(dex_field_index);
  ObjPtr<mirror::String> name = dex_cache->GetResolvedString(field_id.name_idx_);
  if (resolve && name == nullptr) {
    name = ResolveGetStringName(self, *dex_file, field_id.name_idx_, dex_cache);
  }
  return name;
}

template<typename RootVisitorType>
inline void ArtField::VisitRoots(RootVisitorType& visitor) {
  visitor.VisitRoot(declaring_class_.AddressWithoutBarrier());
}

template <typename Visitor>
inline void ArtField::UpdateObjects(const Visitor& visitor) {
  ObjPtr<mirror::Class> old_class = DeclaringClassRoot().Read<kWithoutReadBarrier>();
  ObjPtr<mirror::Class> new_class = visitor(old_class.Ptr());
  if (old_class != new_class) {
    SetDeclaringClass(new_class);
  }
}

// If kExactOffset is true then we only find the matching offset, not the field containing the
// offset.
template <bool kExactOffset>
static inline ArtField* FindFieldWithOffset(
    const IterationRange<StrideIterator<ArtField>>& fields,
    uint32_t field_offset) REQUIRES_SHARED(Locks::mutator_lock_) {
  for (ArtField& field : fields) {
    if (kExactOffset) {
      if (field.GetOffset().Uint32Value() == field_offset) {
        return &field;
      }
    } else {
      const uint32_t offset = field.GetOffset().Uint32Value();
      Primitive::Type type = field.GetTypeAsPrimitiveType();
      const size_t field_size = Primitive::ComponentSize(type);
      DCHECK_GT(field_size, 0u);
      if (offset <= field_offset && field_offset < offset + field_size) {
        return &field;
      }
    }
  }
  return nullptr;
}

template <bool kExactOffset>
inline ArtField* ArtField::FindInstanceFieldWithOffset(ObjPtr<mirror::Class> klass,
                                                       uint32_t field_offset) {
  DCHECK(klass != nullptr);
  ArtField* field = FindFieldWithOffset<kExactOffset>(klass->GetIFields(), field_offset);
  if (field != nullptr) {
    return field;
  }
  // We did not find field in the class: look into superclass.
  return (klass->GetSuperClass() != nullptr) ?
      FindInstanceFieldWithOffset<kExactOffset>(klass->GetSuperClass(), field_offset) : nullptr;
}

template <bool kExactOffset>
inline ArtField* ArtField::FindStaticFieldWithOffset(ObjPtr<mirror::Class> klass,
                                                     uint32_t field_offset) {
  DCHECK(klass != nullptr);
  return FindFieldWithOffset<kExactOffset>(klass->GetSFields(), field_offset);
}

}  // namespace art

#endif  // ART_RUNTIME_ART_FIELD_INL_H_
