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

#ifndef ART_RUNTIME_MIRROR_DEX_CACHE_H_
#define ART_RUNTIME_MIRROR_DEX_CACHE_H_

#include "array.h"
#include "art_field.h"
#include "art_method.h"
#include "class.h"
#include "object.h"
#include "object_array.h"

namespace art {

struct DexCacheOffsets;
class DexFile;
class ImageWriter;
union JValue;

namespace mirror {

class String;

// C++ mirror of java.lang.DexCache.
class MANAGED DexCache FINAL : public Object {
 public:
  // Size of java.lang.DexCache.class.
  static uint32_t ClassSize(size_t pointer_size);

  // Size of an instance of java.lang.DexCache not including referenced values.
  static constexpr uint32_t InstanceSize() {
    return sizeof(DexCache);
  }

  void Init(const DexFile* dex_file,
            String* location,
            GcRoot<String>* strings,
            uint32_t num_strings,
            GcRoot<Class>* resolved_types,
            uint32_t num_resolved_types,
            ArtMethod** resolved_methods,
            uint32_t num_resolved_methods,
            ArtField** resolved_fields,
            uint32_t num_resolved_fields,
            size_t pointer_size) SHARED_REQUIRES(Locks::mutator_lock_);

  void Fixup(ArtMethod* trampoline, size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier, typename Visitor>
  void FixupStrings(GcRoot<mirror::String>* dest, const Visitor& visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier, typename Visitor>
  void FixupResolvedTypes(GcRoot<mirror::Class>* dest, const Visitor& visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);

  String* GetLocation() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_));
  }

  static MemberOffset DexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, dex_);
  }

  static MemberOffset StringsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, strings_);
  }

  static MemberOffset ResolvedTypesOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_types_);
  }

  static MemberOffset ResolvedFieldsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_fields_);
  }

  static MemberOffset ResolvedMethodsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, resolved_methods_);
  }

  static MemberOffset NumStringsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, num_strings_);
  }

  static MemberOffset NumResolvedTypesOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, num_resolved_types_);
  }

  static MemberOffset NumResolvedFieldsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, num_resolved_fields_);
  }

  static MemberOffset NumResolvedMethodsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(DexCache, num_resolved_methods_);
  }

  String* GetResolvedString(uint32_t string_idx) ALWAYS_INLINE
      SHARED_REQUIRES(Locks::mutator_lock_);

  void SetResolvedString(uint32_t string_idx, String* resolved) ALWAYS_INLINE
      SHARED_REQUIRES(Locks::mutator_lock_);

  Class* GetResolvedType(uint32_t type_idx) SHARED_REQUIRES(Locks::mutator_lock_);

  void SetResolvedType(uint32_t type_idx, Class* resolved) SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetResolvedMethod(uint32_t method_idx, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE void SetResolvedMethod(uint32_t method_idx, ArtMethod* resolved, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Pointer sized variant, used for patching.
  ALWAYS_INLINE ArtField* GetResolvedField(uint32_t idx, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Pointer sized variant, used for patching.
  ALWAYS_INLINE void SetResolvedField(uint32_t idx, ArtField* field, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  GcRoot<String>* GetStrings() ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldPtr<GcRoot<String>*>(StringsOffset());
  }

  void SetStrings(GcRoot<String>* strings) ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldPtr<false>(StringsOffset(), strings);
  }

  GcRoot<Class>* GetResolvedTypes() ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldPtr<GcRoot<Class>*>(ResolvedTypesOffset());
  }

  void SetResolvedTypes(GcRoot<Class>* resolved_types)
      ALWAYS_INLINE
      SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldPtr<false>(ResolvedTypesOffset(), resolved_types);
  }

  ArtMethod** GetResolvedMethods() ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldPtr<ArtMethod**>(ResolvedMethodsOffset());
  }

  void SetResolvedMethods(ArtMethod** resolved_methods)
      ALWAYS_INLINE
      SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldPtr<false>(ResolvedMethodsOffset(), resolved_methods);
  }

  ArtField** GetResolvedFields() ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldPtr<ArtField**>(ResolvedFieldsOffset());
  }

  void SetResolvedFields(ArtField** resolved_fields)
      ALWAYS_INLINE
      SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldPtr<false>(ResolvedFieldsOffset(), resolved_fields);
  }

  size_t NumStrings() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(NumStringsOffset());
  }

  size_t NumResolvedTypes() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(NumResolvedTypesOffset());
  }

  size_t NumResolvedMethods() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(NumResolvedMethodsOffset());
  }

  size_t NumResolvedFields() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32(NumResolvedFieldsOffset());
  }

  const DexFile* GetDexFile() ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldPtr<const DexFile*>(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_));
  }

  void SetDexFile(const DexFile* dex_file) SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, dex_file_), dex_file);
  }

  void SetLocation(mirror::String* location) SHARED_REQUIRES(Locks::mutator_lock_);

  // NOTE: Get/SetElementPtrSize() are intended for working with ArtMethod** and ArtField**
  // provided by GetResolvedMethods/Fields() and ArtMethod::GetDexCacheResolvedMethods(),
  // so they need to be public.

  template <typename PtrType>
  static PtrType GetElementPtrSize(PtrType* ptr_array, size_t idx, size_t ptr_size);

  template <typename PtrType>
  static void SetElementPtrSize(PtrType* ptr_array, size_t idx, PtrType ptr, size_t ptr_size);

 private:
  // Visit instance fields of the dex cache as well as its associated arrays.
  template <bool kVisitNativeRoots,
            VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor>
  void VisitReferences(mirror::Class* klass, const Visitor& visitor)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);

  HeapReference<Object> dex_;
  HeapReference<String> location_;
  uint64_t dex_file_;           // const DexFile*
  uint64_t resolved_fields_;    // ArtField*, array with num_resolved_fields_ elements.
  uint64_t resolved_methods_;   // ArtMethod*, array with num_resolved_methods_ elements.
  uint64_t resolved_types_;     // GcRoot<Class>*, array with num_resolved_types_ elements.
  uint64_t strings_;            // GcRoot<String>*, array with num_strings_ elements.
  uint32_t num_resolved_fields_;    // Number of elements in the resolved_fields_ array.
  uint32_t num_resolved_methods_;   // Number of elements in the resolved_methods_ array.
  uint32_t num_resolved_types_;     // Number of elements in the resolved_types_ array.
  uint32_t num_strings_;            // Number of elements in the strings_ array.

  friend struct art::DexCacheOffsets;  // for verifying offset information
  friend class Object;  // For VisitReferences
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexCache);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_H_
