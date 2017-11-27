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

#ifndef ART_RUNTIME_ART_FIELD_H_
#define ART_RUNTIME_ART_FIELD_H_

#include <jni.h>

#include "gc_root.h"
#include "modifiers.h"
#include "offsets.h"
#include "primitive.h"
#include "read_barrier_option.h"

namespace art {

class DexFile;
class ScopedObjectAccessAlreadyRunnable;

namespace mirror {
class Class;
class DexCache;
class Object;
class String;
}  // namespace mirror

class ArtField FINAL {
 public:
  ArtField();

  mirror::Class* GetDeclaringClass() SHARED_REQUIRES(Locks::mutator_lock_);

  void SetDeclaringClass(mirror::Class *new_declaring_class)
      SHARED_REQUIRES(Locks::mutator_lock_);

  uint32_t GetAccessFlags() SHARED_REQUIRES(Locks::mutator_lock_);

  void SetAccessFlags(uint32_t new_access_flags) SHARED_REQUIRES(Locks::mutator_lock_) {
    // Not called within a transaction.
    access_flags_ = new_access_flags;
  }

  bool IsPublic() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  bool IsStatic() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  bool IsFinal() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  uint32_t GetDexFieldIndex() {
    return field_dex_idx_;
  }

  void SetDexFieldIndex(uint32_t new_idx) {
    // Not called within a transaction.
    field_dex_idx_ = new_idx;
  }

  // Offset to field within an Object.
  MemberOffset GetOffset() SHARED_REQUIRES(Locks::mutator_lock_);

  static MemberOffset OffsetOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ArtField, offset_));
  }

  MemberOffset GetOffsetDuringLinking() SHARED_REQUIRES(Locks::mutator_lock_);

  void SetOffset(MemberOffset num_bytes) SHARED_REQUIRES(Locks::mutator_lock_);

  // field access, null object for static fields
  uint8_t GetBoolean(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetBoolean(mirror::Object* object, uint8_t z) SHARED_REQUIRES(Locks::mutator_lock_);

  int8_t GetByte(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetByte(mirror::Object* object, int8_t b) SHARED_REQUIRES(Locks::mutator_lock_);

  uint16_t GetChar(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetChar(mirror::Object* object, uint16_t c) SHARED_REQUIRES(Locks::mutator_lock_);

  int16_t GetShort(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetShort(mirror::Object* object, int16_t s) SHARED_REQUIRES(Locks::mutator_lock_);

  int32_t GetInt(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetInt(mirror::Object* object, int32_t i) SHARED_REQUIRES(Locks::mutator_lock_);

  int64_t GetLong(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetLong(mirror::Object* object, int64_t j) SHARED_REQUIRES(Locks::mutator_lock_);

  float GetFloat(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetFloat(mirror::Object* object, float f) SHARED_REQUIRES(Locks::mutator_lock_);

  double GetDouble(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetDouble(mirror::Object* object, double d) SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::Object* GetObject(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetObject(mirror::Object* object, mirror::Object* l)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Raw field accesses.
  uint32_t Get32(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void Set32(mirror::Object* object, uint32_t new_value)
      SHARED_REQUIRES(Locks::mutator_lock_);

  uint64_t Get64(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void Set64(mirror::Object* object, uint64_t new_value) SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::Object* GetObj(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive>
  void SetObj(mirror::Object* object, mirror::Object* new_value)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // NO_THREAD_SAFETY_ANALYSIS since we don't know what the callback requires.
  template<typename RootVisitorType>
  void VisitRoots(RootVisitorType& visitor) NO_THREAD_SAFETY_ANALYSIS;

  bool IsVolatile() SHARED_REQUIRES(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccVolatile) != 0;
  }

  // Returns an instance field with this offset in the given class or null if not found.
  // If kExactOffset is true then we only find the matching offset, not the field containing the
  // offset.
  template <bool kExactOffset = true>
  static ArtField* FindInstanceFieldWithOffset(mirror::Class* klass, uint32_t field_offset)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns a static field with this offset in the given class or null if not found.
  // If kExactOffset is true then we only find the matching offset, not the field containing the
  // offset.
  template <bool kExactOffset = true>
  static ArtField* FindStaticFieldWithOffset(mirror::Class* klass, uint32_t field_offset)
      SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetName() SHARED_REQUIRES(Locks::mutator_lock_);

  // Resolves / returns the name from the dex cache.
  mirror::String* GetStringName(Thread* self, bool resolve)
      SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetTypeDescriptor() SHARED_REQUIRES(Locks::mutator_lock_);

  Primitive::Type GetTypeAsPrimitiveType() SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsPrimitiveType() SHARED_REQUIRES(Locks::mutator_lock_);

  template <bool kResolve>
  mirror::Class* GetType() SHARED_REQUIRES(Locks::mutator_lock_);

  size_t FieldSize() SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::DexCache* GetDexCache() SHARED_REQUIRES(Locks::mutator_lock_);

  const DexFile* GetDexFile() SHARED_REQUIRES(Locks::mutator_lock_);

  GcRoot<mirror::Class>& DeclaringClassRoot() {
    return declaring_class_;
  }

  // Update the declaring class with the passed in visitor. Does not use read barrier.
  template <typename Visitor>
  ALWAYS_INLINE void UpdateObjects(const Visitor& visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  mirror::Class* ProxyFindSystemClass(const char* descriptor)
      SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::Class* ResolveGetType(uint32_t type_idx) SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::String* ResolveGetStringName(Thread* self, const DexFile& dex_file, uint32_t string_idx,
                                       mirror::DexCache* dex_cache)
      SHARED_REQUIRES(Locks::mutator_lock_);

  GcRoot<mirror::Class> declaring_class_;

  uint32_t access_flags_;

  // Dex cache index of field id
  uint32_t field_dex_idx_;

  // Offset of field within an instance or in the Class' static fields
  uint32_t offset_;
};

}  // namespace art

#endif  // ART_RUNTIME_ART_FIELD_H_
