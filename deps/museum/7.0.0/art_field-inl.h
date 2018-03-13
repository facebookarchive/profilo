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
#include "gc_root-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "jvalue.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "primitive.h"
#include "thread-inl.h"
#include "scoped_thread_state_change.h"
#include "well_known_classes.h"

namespace art {

inline mirror::Class* ArtField::GetDeclaringClass() {
  GcRootSource gc_root_source(this);
  mirror::Class* result = declaring_class_.Read(&gc_root_source);
  DCHECK(result != nullptr);
  DCHECK(result->IsLoaded() || result->IsErroneous()) << result->GetStatus();
  return result;
}

inline void ArtField::SetDeclaringClass(mirror::Class* new_declaring_class) {
  declaring_class_ = GcRoot<mirror::Class>(new_declaring_class);
}

inline uint32_t ArtField::GetAccessFlags() {
  DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
  return access_flags_;
}

inline MemberOffset ArtField::GetOffset() {
  DCHECK(GetDeclaringClass()->IsResolved() || GetDeclaringClass()->IsErroneous());
  return MemberOffset(offset_);
}

inline MemberOffset ArtField::GetOffsetDuringLinking() {
  DCHECK(GetDeclaringClass()->IsLoaded() || GetDeclaringClass()->IsErroneous());
  return MemberOffset(offset_);
}

inline uint32_t ArtField::Get32(mirror::Object* object) {
  DCHECK(object != nullptr) << PrettyField(this);
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    return object->GetField32Volatile(GetOffset());
  }
  return object->GetField32(GetOffset());
}

template<bool kTransactionActive>
inline void ArtField::Set32(mirror::Object* object, uint32_t new_value) {
  DCHECK(object != nullptr) << PrettyField(this);
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    object->SetField32Volatile<kTransactionActive>(GetOffset(), new_value);
  } else {
    object->SetField32<kTransactionActive>(GetOffset(), new_value);
  }
}

inline uint64_t ArtField::Get64(mirror::Object* object) {
  DCHECK(object != nullptr) << PrettyField(this);
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    return object->GetField64Volatile(GetOffset());
  }
  return object->GetField64(GetOffset());
}

template<bool kTransactionActive>
inline void ArtField::Set64(mirror::Object* object, uint64_t new_value) {
  DCHECK(object != nullptr) << PrettyField(this);
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    object->SetField64Volatile<kTransactionActive>(GetOffset(), new_value);
  } else {
    object->SetField64<kTransactionActive>(GetOffset(), new_value);
  }
}

inline mirror::Object* ArtField::GetObj(mirror::Object* object) {
  DCHECK(object != nullptr) << PrettyField(this);
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    return object->GetFieldObjectVolatile<mirror::Object>(GetOffset());
  }
  return object->GetFieldObject<mirror::Object>(GetOffset());
}

template<bool kTransactionActive>
inline void ArtField::SetObj(mirror::Object* object, mirror::Object* new_value) {
  DCHECK(object != nullptr) << PrettyField(this);
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted());
  if (UNLIKELY(IsVolatile())) {
    object->SetFieldObjectVolatile<kTransactionActive>(GetOffset(), new_value);
  } else {
    object->SetFieldObject<kTransactionActive>(GetOffset(), new_value);
  }
}

#define FIELD_GET(object, type) \
  DCHECK_EQ(Primitive::kPrim ## type, GetTypeAsPrimitiveType()) << PrettyField(this); \
  DCHECK(object != nullptr) << PrettyField(this); \
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted()); \
  if (UNLIKELY(IsVolatile())) { \
    return object->GetField ## type ## Volatile(GetOffset()); \
  } \
  return object->GetField ## type(GetOffset());

#define FIELD_SET(object, type, value) \
  DCHECK_EQ(Primitive::kPrim ## type, GetTypeAsPrimitiveType()) << PrettyField(this); \
  DCHECK(object != nullptr) << PrettyField(this); \
  DCHECK(!IsStatic() || (object == GetDeclaringClass()) || !Runtime::Current()->IsStarted()); \
  if (UNLIKELY(IsVolatile())) { \
    object->SetField ## type ## Volatile<kTransactionActive>(GetOffset(), value); \
  } else { \
    object->SetField ## type<kTransactionActive>(GetOffset(), value); \
  }

inline uint8_t ArtField::GetBoolean(mirror::Object* object) {
  FIELD_GET(object, Boolean);
}

template<bool kTransactionActive>
inline void ArtField::SetBoolean(mirror::Object* object, uint8_t z) {
  FIELD_SET(object, Boolean, z);
}

inline int8_t ArtField::GetByte(mirror::Object* object) {
  FIELD_GET(object, Byte);
}

template<bool kTransactionActive>
inline void ArtField::SetByte(mirror::Object* object, int8_t b) {
  FIELD_SET(object, Byte, b);
}

inline uint16_t ArtField::GetChar(mirror::Object* object) {
  FIELD_GET(object, Char);
}

template<bool kTransactionActive>
inline void ArtField::SetChar(mirror::Object* object, uint16_t c) {
  FIELD_SET(object, Char, c);
}

inline int16_t ArtField::GetShort(mirror::Object* object) {
  FIELD_GET(object, Short);
}

template<bool kTransactionActive>
inline void ArtField::SetShort(mirror::Object* object, int16_t s) {
  FIELD_SET(object, Short, s);
}

#undef FIELD_GET
#undef FIELD_SET

inline int32_t ArtField::GetInt(mirror::Object* object) {
  if (kIsDebugBuild) {
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimInt || type == Primitive::kPrimFloat) << PrettyField(this);
  }
  return Get32(object);
}

template<bool kTransactionActive>
inline void ArtField::SetInt(mirror::Object* object, int32_t i) {
  if (kIsDebugBuild) {
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimInt || type == Primitive::kPrimFloat) << PrettyField(this);
  }
  Set32<kTransactionActive>(object, i);
}

inline int64_t ArtField::GetLong(mirror::Object* object) {
  if (kIsDebugBuild) {
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimLong || type == Primitive::kPrimDouble) << PrettyField(this);
  }
  return Get64(object);
}

template<bool kTransactionActive>
inline void ArtField::SetLong(mirror::Object* object, int64_t j) {
  if (kIsDebugBuild) {
    Primitive::Type type = GetTypeAsPrimitiveType();
    CHECK(type == Primitive::kPrimLong || type == Primitive::kPrimDouble) << PrettyField(this);
  }
  Set64<kTransactionActive>(object, j);
}

inline float ArtField::GetFloat(mirror::Object* object) {
  DCHECK_EQ(Primitive::kPrimFloat, GetTypeAsPrimitiveType()) << PrettyField(this);
  JValue bits;
  bits.SetI(Get32(object));
  return bits.GetF();
}

template<bool kTransactionActive>
inline void ArtField::SetFloat(mirror::Object* object, float f) {
  DCHECK_EQ(Primitive::kPrimFloat, GetTypeAsPrimitiveType()) << PrettyField(this);
  JValue bits;
  bits.SetF(f);
  Set32<kTransactionActive>(object, bits.GetI());
}

inline double ArtField::GetDouble(mirror::Object* object) {
  DCHECK_EQ(Primitive::kPrimDouble, GetTypeAsPrimitiveType()) << PrettyField(this);
  JValue bits;
  bits.SetJ(Get64(object));
  return bits.GetD();
}

template<bool kTransactionActive>
inline void ArtField::SetDouble(mirror::Object* object, double d) {
  DCHECK_EQ(Primitive::kPrimDouble, GetTypeAsPrimitiveType()) << PrettyField(this);
  JValue bits;
  bits.SetD(d);
  Set64<kTransactionActive>(object, bits.GetJ());
}

inline mirror::Object* ArtField::GetObject(mirror::Object* object) {
  DCHECK_EQ(Primitive::kPrimNot, GetTypeAsPrimitiveType()) << PrettyField(this);
  return GetObj(object);
}

template<bool kTransactionActive>
inline void ArtField::SetObject(mirror::Object* object, mirror::Object* l) {
  DCHECK_EQ(Primitive::kPrimNot, GetTypeAsPrimitiveType()) << PrettyField(this);
  SetObj<kTransactionActive>(object, l);
}

inline const char* ArtField::GetName() SHARED_REQUIRES(Locks::mutator_lock_) {
  uint32_t field_index = GetDexFieldIndex();
  if (UNLIKELY(GetDeclaringClass()->IsProxyClass())) {
    DCHECK(IsStatic());
    DCHECK_LT(field_index, 2U);
    return field_index == 0 ? "interfaces" : "throws";
  }
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetFieldName(dex_file->GetFieldId(field_index));
}

inline const char* ArtField::GetTypeDescriptor() SHARED_REQUIRES(Locks::mutator_lock_) {
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
    SHARED_REQUIRES(Locks::mutator_lock_) {
  return Primitive::GetType(GetTypeDescriptor()[0]);
}

inline bool ArtField::IsPrimitiveType() SHARED_REQUIRES(Locks::mutator_lock_) {
  return GetTypeAsPrimitiveType() != Primitive::kPrimNot;
}

template <bool kResolve>
inline mirror::Class* ArtField::GetType() {
  const uint32_t field_index = GetDexFieldIndex();
  auto* declaring_class = GetDeclaringClass();
  if (UNLIKELY(declaring_class->IsProxyClass())) {
    return ProxyFindSystemClass(GetTypeDescriptor());
  }
  auto* dex_cache = declaring_class->GetDexCache();
  const DexFile* const dex_file = dex_cache->GetDexFile();
  const DexFile::FieldId& field_id = dex_file->GetFieldId(field_index);
  mirror::Class* type = dex_cache->GetResolvedType(field_id.type_idx_);
  if (kResolve && UNLIKELY(type == nullptr)) {
    type = ResolveGetType(field_id.type_idx_);
    CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
  }
  return type;
}

inline size_t ArtField::FieldSize() SHARED_REQUIRES(Locks::mutator_lock_) {
  return Primitive::ComponentSize(GetTypeAsPrimitiveType());
}

inline mirror::DexCache* ArtField::GetDexCache() SHARED_REQUIRES(Locks::mutator_lock_) {
  return GetDeclaringClass()->GetDexCache();
}

inline const DexFile* ArtField::GetDexFile() SHARED_REQUIRES(Locks::mutator_lock_) {
  return GetDexCache()->GetDexFile();
}

inline mirror::String* ArtField::GetStringName(Thread* self, bool resolve) {
  auto dex_field_index = GetDexFieldIndex();
  CHECK_NE(dex_field_index, DexFile::kDexNoIndex);
  auto* dex_cache = GetDexCache();
  const auto* dex_file = dex_cache->GetDexFile();
  const auto& field_id = dex_file->GetFieldId(dex_field_index);
  auto* name = dex_cache->GetResolvedString(field_id.name_idx_);
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
  mirror::Class* old_class = DeclaringClassRoot().Read<kWithoutReadBarrier>();
  mirror::Class* new_class = visitor(old_class);
  if (old_class != new_class) {
    SetDeclaringClass(new_class);
  }
}

// If kExactOffset is true then we only find the matching offset, not the field containing the
// offset.
template <bool kExactOffset>
static inline ArtField* FindFieldWithOffset(
    const IterationRange<StrideIterator<ArtField>>& fields,
    uint32_t field_offset) SHARED_REQUIRES(Locks::mutator_lock_) {
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
inline ArtField* ArtField::FindInstanceFieldWithOffset(mirror::Class* klass,
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
inline ArtField* ArtField::FindStaticFieldWithOffset(mirror::Class* klass, uint32_t field_offset) {
  DCHECK(klass != nullptr);
  return FindFieldWithOffset<kExactOffset>(klass->GetSFields(), field_offset);
}

}  // namespace art

#endif  // ART_RUNTIME_ART_FIELD_INL_H_
