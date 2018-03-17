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

#ifndef ART_RUNTIME_MIRROR_FIELD_H_
#define ART_RUNTIME_MIRROR_FIELD_H_

#include "accessible_object.h"
#include "base/enums.h"
#include "gc_root.h"
#include "obj_ptr.h"
#include "object.h"
#include "object_callbacks.h"
#include "read_barrier_option.h"

namespace art {

class ArtField;
struct FieldOffsets;

namespace mirror {

class Class;
class String;

// C++ mirror of java.lang.reflect.Field.
class MANAGED Field : public AccessibleObject {
 public:
  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  static mirror::Class* ArrayClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return array_class_.Read();
  }

  ALWAYS_INLINE uint32_t GetDexFieldIndex() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Field, dex_field_index_));
  }

  mirror::Class* GetDeclaringClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(Field, declaring_class_));
  }

  uint32_t GetAccessFlags() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Field, access_flags_));
  }

  bool IsStatic() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  bool IsFinal() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  bool IsVolatile() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccVolatile) != 0;
  }

  ALWAYS_INLINE Primitive::Type GetTypeAsPrimitiveType()
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetType()->GetPrimitiveType();
  }

  mirror::Class* GetType() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<mirror::Class>(OFFSET_OF_OBJECT_MEMBER(Field, type_));
  }

  int32_t GetOffset() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Field, offset_));
  }

  static void SetClass(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static void SetArrayClass(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetArrayClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Slow, try to use only for PrettyField and such.
  ArtField* GetArtField() REQUIRES_SHARED(Locks::mutator_lock_);

  template <PointerSize kPointerSize, bool kTransactionActive = false>
  static mirror::Field* CreateFromArtField(Thread* self, ArtField* field,
                                           bool force_resolve)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

 private:
  // Padding required for matching alignment with the Java peer.
  uint8_t padding_[2];

  HeapReference<mirror::Class> declaring_class_;
  HeapReference<mirror::Class> type_;
  int32_t access_flags_;
  int32_t dex_field_index_;
  int32_t offset_;

  template<bool kTransactionActive>
  void SetDeclaringClass(ObjPtr<mirror::Class> c) REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetType(ObjPtr<mirror::Class> type) REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetAccessFlags(uint32_t flags) REQUIRES_SHARED(Locks::mutator_lock_) {
    SetField32<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, access_flags_), flags);
  }

  template<bool kTransactionActive>
  void SetDexFieldIndex(uint32_t idx) REQUIRES_SHARED(Locks::mutator_lock_) {
    SetField32<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, dex_field_index_), idx);
  }

  template<bool kTransactionActive>
  void SetOffset(uint32_t offset) REQUIRES_SHARED(Locks::mutator_lock_) {
    SetField32<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, offset_), offset);
  }

  static GcRoot<Class> static_class_;  // java.lang.reflect.Field.class.
  static GcRoot<Class> array_class_;  // array of java.lang.reflect.Field.

  friend struct art::FieldOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Field);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_FIELD_H_
