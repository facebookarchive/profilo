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
#include "gc_root.h"
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
  static mirror::Class* StaticClass() SHARED_REQUIRES(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  static mirror::Class* ArrayClass() SHARED_REQUIRES(Locks::mutator_lock_) {
    return array_class_.Read();
  }

  ALWAYS_INLINE uint32_t GetDexFieldIndex() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Field, dex_field_index_));
  }

  mirror::Class* GetDeclaringClass() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(Field, declaring_class_));
  }

  uint32_t GetAccessFlags() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Field, access_flags_));
  }

  bool IsStatic() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  bool IsFinal() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  bool IsVolatile() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccVolatile) != 0;
  }

  ALWAYS_INLINE Primitive::Type GetTypeAsPrimitiveType()
      SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetType()->GetPrimitiveType();
  }

  mirror::Class* GetType() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObject<mirror::Class>(OFFSET_OF_OBJECT_MEMBER(Field, type_));
  }

  int32_t GetOffset() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Field, offset_));
  }

  static void SetClass(Class* klass) SHARED_REQUIRES(Locks::mutator_lock_);
  static void ResetClass() SHARED_REQUIRES(Locks::mutator_lock_);

  static void SetArrayClass(Class* klass) SHARED_REQUIRES(Locks::mutator_lock_);
  static void ResetArrayClass() SHARED_REQUIRES(Locks::mutator_lock_);

  static void VisitRoots(RootVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_);

  // Slow, try to use only for PrettyField and such.
  ArtField* GetArtField() SHARED_REQUIRES(Locks::mutator_lock_);

  template <bool kTransactionActive = false>
  static mirror::Field* CreateFromArtField(Thread* self, ArtField* field,
                                           bool force_resolve)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

 private:
  HeapReference<mirror::Class> declaring_class_;
  HeapReference<mirror::Class> type_;
  int32_t access_flags_;
  int32_t dex_field_index_;
  int32_t offset_;

  template<bool kTransactionActive>
  void SetDeclaringClass(mirror::Class* c) SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldObject<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, declaring_class_), c);
  }

  template<bool kTransactionActive>
  void SetType(mirror::Class* type) SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldObject<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, type_), type);
  }

  template<bool kTransactionActive>
  void SetAccessFlags(uint32_t flags) SHARED_REQUIRES(Locks::mutator_lock_) {
    SetField32<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, access_flags_), flags);
  }

  template<bool kTransactionActive>
  void SetDexFieldIndex(uint32_t idx) SHARED_REQUIRES(Locks::mutator_lock_) {
    SetField32<kTransactionActive>(OFFSET_OF_OBJECT_MEMBER(Field, dex_field_index_), idx);
  }

  template<bool kTransactionActive>
  void SetOffset(uint32_t offset) SHARED_REQUIRES(Locks::mutator_lock_) {
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
