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

#ifndef ART_RUNTIME_MIRROR_CLASS_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_INL_H_

#include "class.h"

#include "art_field-inl.h"
#include "art_method.h"
#include "art_method-inl.h"
#include "class_loader.h"
#include "common_throws.h"
#include "dex_cache.h"
#include "dex_file.h"
#include "gc/heap-inl.h"
#include "iftable.h"
#include "object_array-inl.h"
#include "read_barrier-inl.h"
#include "reference-inl.h"
#include "runtime.h"
#include "string.h"
#include "utils.h"

namespace art {
namespace mirror {

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline uint32_t Class::GetObjectSize() {
  // Note: Extra parentheses to avoid the comma being interpreted as macro parameter separator.
  DCHECK((!IsVariableSize<kVerifyFlags, kReadBarrierOption>())) << " class=" << PrettyTypeOf(this);
  return GetField32(ObjectSizeOffset());
}

inline Class* Class::GetSuperClass() {
  // Can only get super class for loaded classes (hack for when runtime is
  // initializing)
  DCHECK(IsLoaded() || IsErroneous() || !Runtime::Current()->IsStarted()) << IsLoaded();
  return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(Class, super_class_));
}

inline ClassLoader* Class::GetClassLoader() {
  return GetFieldObject<ClassLoader>(OFFSET_OF_OBJECT_MEMBER(Class, class_loader_));
}

template<VerifyObjectFlags kVerifyFlags>
inline DexCache* Class::GetDexCache() {
  return GetFieldObject<DexCache, kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_));
}

inline ArtMethod* Class::GetDirectMethodsPtr() {
  DCHECK(IsLoaded() || IsErroneous());
  return GetDirectMethodsPtrUnchecked();
}

inline ArtMethod* Class::GetDirectMethodsPtrUnchecked() {
  return reinterpret_cast<ArtMethod*>(GetField64(OFFSET_OF_OBJECT_MEMBER(Class, direct_methods_)));
}

inline ArtMethod* Class::GetVirtualMethodsPtrUnchecked() {
  return reinterpret_cast<ArtMethod*>(GetField64(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_)));
}

inline void Class::SetDirectMethodsPtr(ArtMethod* new_direct_methods) {
  DCHECK(GetDirectMethodsPtrUnchecked() == nullptr);
  SetDirectMethodsPtrUnchecked(new_direct_methods);
}

inline void Class::SetDirectMethodsPtrUnchecked(ArtMethod* new_direct_methods) {
  SetField64<false>(OFFSET_OF_OBJECT_MEMBER(Class, direct_methods_),
                    reinterpret_cast<uint64_t>(new_direct_methods));
}

inline ArtMethod* Class::GetDirectMethodUnchecked(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  auto* methods = GetDirectMethodsPtrUnchecked();
  DCHECK(methods != nullptr);
  return reinterpret_cast<ArtMethod*>(reinterpret_cast<uintptr_t>(methods) +
      ArtMethod::ObjectSize(pointer_size) * i);
}

inline ArtMethod* Class::GetDirectMethod(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  auto* methods = GetDirectMethodsPtr();
  DCHECK(methods != nullptr);
  return reinterpret_cast<ArtMethod*>(reinterpret_cast<uintptr_t>(methods) +
      ArtMethod::ObjectSize(pointer_size) * i);
}

template<VerifyObjectFlags kVerifyFlags>
inline ArtMethod* Class::GetVirtualMethodsPtr() {
  DCHECK(IsLoaded<kVerifyFlags>() || IsErroneous<kVerifyFlags>());
  return GetVirtualMethodsPtrUnchecked();
}

inline void Class::SetVirtualMethodsPtr(ArtMethod* new_virtual_methods) {
  // TODO: we reassign virtual methods to grow the table for miranda
  // methods.. they should really just be assigned once.
  SetField64<false>(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_),
                    reinterpret_cast<uint64_t>(new_virtual_methods));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArtMethod* Class::GetVirtualMethod(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  DCHECK(IsResolved<kVerifyFlags>() || IsErroneous<kVerifyFlags>())
      << PrettyClass(this) << " status=" << GetStatus();
  return GetVirtualMethodUnchecked(i, pointer_size);
}

inline ArtMethod* Class::GetVirtualMethodDuringLinking(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  DCHECK(IsLoaded() || IsErroneous());
  return GetVirtualMethodUnchecked(i, pointer_size);
}

inline ArtMethod* Class::GetVirtualMethodUnchecked(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  auto* methods = GetVirtualMethodsPtrUnchecked();
  DCHECK(methods != nullptr);
  return reinterpret_cast<ArtMethod*>(reinterpret_cast<uintptr_t>(methods) +
      ArtMethod::ObjectSize(pointer_size) * i);
}

inline PointerArray* Class::GetVTable() {
  DCHECK(IsResolved() || IsErroneous());
  return GetFieldObject<PointerArray>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_));
}

inline PointerArray* Class::GetVTableDuringLinking() {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<PointerArray>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_));
}

inline void Class::SetVTable(PointerArray* new_vtable) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_), new_vtable);
}

inline MemberOffset Class::EmbeddedImTableEntryOffset(uint32_t i, size_t pointer_size) {
  DCHECK_LT(i, kImtSize);
  return MemberOffset(
      EmbeddedImTableOffset(pointer_size).Uint32Value() + i * ImTableEntrySize(pointer_size));
}

inline ArtMethod* Class::GetEmbeddedImTableEntry(uint32_t i, size_t pointer_size) {
  DCHECK(ShouldHaveEmbeddedImtAndVTable());
  return GetFieldPtrWithSize<ArtMethod*>(
      EmbeddedImTableEntryOffset(i, pointer_size), pointer_size);
}

inline void Class::SetEmbeddedImTableEntry(uint32_t i, ArtMethod* method, size_t pointer_size) {
  DCHECK(ShouldHaveEmbeddedImtAndVTable());
  SetFieldPtrWithSize<false>(EmbeddedImTableEntryOffset(i, pointer_size), method, pointer_size);
}

inline bool Class::HasVTable() {
  return GetVTable() != nullptr || ShouldHaveEmbeddedImtAndVTable();
}

inline int32_t Class::GetVTableLength() {
  if (ShouldHaveEmbeddedImtAndVTable()) {
    return GetEmbeddedVTableLength();
  }
  return GetVTable() != nullptr ? GetVTable()->GetLength() : 0;
}

inline ArtMethod* Class::GetVTableEntry(uint32_t i, size_t pointer_size) {
  if (ShouldHaveEmbeddedImtAndVTable()) {
    return GetEmbeddedVTableEntry(i, pointer_size);
  }
  auto* vtable = GetVTable();
  DCHECK(vtable != nullptr);
  return vtable->GetElementPtrSize<ArtMethod*>(i, pointer_size);
}

inline int32_t Class::GetEmbeddedVTableLength() {
  return GetField32(MemberOffset(EmbeddedVTableLengthOffset()));
}

inline void Class::SetEmbeddedVTableLength(int32_t len) {
  SetField32<false>(MemberOffset(EmbeddedVTableLengthOffset()), len);
}

inline MemberOffset Class::EmbeddedVTableEntryOffset(uint32_t i, size_t pointer_size) {
  return MemberOffset(
      EmbeddedVTableOffset(pointer_size).Uint32Value() + i * VTableEntrySize(pointer_size));
}

inline ArtMethod* Class::GetEmbeddedVTableEntry(uint32_t i, size_t pointer_size) {
  return GetFieldPtrWithSize<ArtMethod*>(EmbeddedVTableEntryOffset(i, pointer_size), pointer_size);
}

inline void Class::SetEmbeddedVTableEntryUnchecked(
    uint32_t i, ArtMethod* method, size_t pointer_size) {
  SetFieldPtrWithSize<false>(EmbeddedVTableEntryOffset(i, pointer_size), method, pointer_size);
}

inline void Class::SetEmbeddedVTableEntry(uint32_t i, ArtMethod* method, size_t pointer_size) {
  auto* vtable = GetVTableDuringLinking();
  CHECK_EQ(method, vtable->GetElementPtrSize<ArtMethod*>(i, pointer_size));
  SetEmbeddedVTableEntryUnchecked(i, method, pointer_size);
}

inline bool Class::Implements(Class* klass) {
  DCHECK(klass != nullptr);
  DCHECK(klass->IsInterface()) << PrettyClass(this);
  // All interfaces implemented directly and by our superclass, and
  // recursively all super-interfaces of those interfaces, are listed
  // in iftable_, so we can just do a linear scan through that.
  int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    if (iftable->GetInterface(i) == klass) {
      return true;
    }
  }
  return false;
}

// Determine whether "this" is assignable from "src", where both of these
// are array classes.
//
// Consider an array class, e.g. Y[][], where Y is a subclass of X.
//   Y[][]            = Y[][] --> true (identity)
//   X[][]            = Y[][] --> true (element superclass)
//   Y                = Y[][] --> false
//   Y[]              = Y[][] --> false
//   Object           = Y[][] --> true (everything is an object)
//   Object[]         = Y[][] --> true
//   Object[][]       = Y[][] --> true
//   Object[][][]     = Y[][] --> false (too many []s)
//   Serializable     = Y[][] --> true (all arrays are Serializable)
//   Serializable[]   = Y[][] --> true
//   Serializable[][] = Y[][] --> false (unless Y is Serializable)
//
// Don't forget about primitive types.
//   Object[]         = int[] --> false
//
inline bool Class::IsArrayAssignableFromArray(Class* src) {
  DCHECK(IsArrayClass())  << PrettyClass(this);
  DCHECK(src->IsArrayClass()) << PrettyClass(src);
  return GetComponentType()->IsAssignableFrom(src->GetComponentType());
}

inline bool Class::IsAssignableFromArray(Class* src) {
  DCHECK(!IsInterface()) << PrettyClass(this);  // handled first in IsAssignableFrom
  DCHECK(src->IsArrayClass()) << PrettyClass(src);
  if (!IsArrayClass()) {
    // If "this" is not also an array, it must be Object.
    // src's super should be java_lang_Object, since it is an array.
    Class* java_lang_Object = src->GetSuperClass();
    DCHECK(java_lang_Object != nullptr) << PrettyClass(src);
    DCHECK(java_lang_Object->GetSuperClass() == nullptr) << PrettyClass(src);
    return this == java_lang_Object;
  }
  return IsArrayAssignableFromArray(src);
}

template <bool throw_on_failure, bool use_referrers_cache>
inline bool Class::ResolvedFieldAccessTest(Class* access_to, ArtField* field,
                                           uint32_t field_idx, DexCache* dex_cache) {
  DCHECK_EQ(use_referrers_cache, dex_cache == nullptr);
  if (UNLIKELY(!this->CanAccess(access_to))) {
    // The referrer class can't access the field's declaring class but may still be able
    // to access the field if the FieldId specifies an accessible subclass of the declaring
    // class rather than the declaring class itself.
    DexCache* referrer_dex_cache = use_referrers_cache ? this->GetDexCache() : dex_cache;
    uint32_t class_idx = referrer_dex_cache->GetDexFile()->GetFieldId(field_idx).class_idx_;
    // The referenced class has already been resolved with the field, get it from the dex cache.
    Class* dex_access_to = referrer_dex_cache->GetResolvedType(class_idx);
    DCHECK(dex_access_to != nullptr);
    if (UNLIKELY(!this->CanAccess(dex_access_to))) {
      if (throw_on_failure) {
        ThrowIllegalAccessErrorClass(this, dex_access_to);
      }
      return false;
    }
    DCHECK_EQ(this->CanAccessMember(access_to, field->GetAccessFlags()),
              this->CanAccessMember(dex_access_to, field->GetAccessFlags()));
  }
  if (LIKELY(this->CanAccessMember(access_to, field->GetAccessFlags()))) {
    return true;
  }
  if (throw_on_failure) {
    ThrowIllegalAccessErrorField(this, field);
  }
  return false;
}

template <bool throw_on_failure, bool use_referrers_cache, InvokeType throw_invoke_type>
inline bool Class::ResolvedMethodAccessTest(Class* access_to, ArtMethod* method,
                                            uint32_t method_idx, DexCache* dex_cache) {
  static_assert(throw_on_failure || throw_invoke_type == kStatic, "Non-default throw invoke type");
  DCHECK_EQ(use_referrers_cache, dex_cache == nullptr);
  if (UNLIKELY(!this->CanAccess(access_to))) {
    // The referrer class can't access the method's declaring class but may still be able
    // to access the method if the MethodId specifies an accessible subclass of the declaring
    // class rather than the declaring class itself.
    DexCache* referrer_dex_cache = use_referrers_cache ? this->GetDexCache() : dex_cache;
    uint32_t class_idx = referrer_dex_cache->GetDexFile()->GetMethodId(method_idx).class_idx_;
    // The referenced class has already been resolved with the method, get it from the dex cache.
    Class* dex_access_to = referrer_dex_cache->GetResolvedType(class_idx);
    DCHECK(dex_access_to != nullptr);
    if (UNLIKELY(!this->CanAccess(dex_access_to))) {
      if (throw_on_failure) {
        ThrowIllegalAccessErrorClassForMethodDispatch(this, dex_access_to,
                                                      method, throw_invoke_type);
      }
      return false;
    }
    DCHECK_EQ(this->CanAccessMember(access_to, method->GetAccessFlags()),
              this->CanAccessMember(dex_access_to, method->GetAccessFlags()));
  }
  if (LIKELY(this->CanAccessMember(access_to, method->GetAccessFlags()))) {
    return true;
  }
  if (throw_on_failure) {
    ThrowIllegalAccessErrorMethod(this, method);
  }
  return false;
}

inline bool Class::CanAccessResolvedField(Class* access_to, ArtField* field,
                                          DexCache* dex_cache, uint32_t field_idx) {
  return ResolvedFieldAccessTest<false, false>(access_to, field, field_idx, dex_cache);
}

inline bool Class::CheckResolvedFieldAccess(Class* access_to, ArtField* field,
                                            uint32_t field_idx) {
  return ResolvedFieldAccessTest<true, true>(access_to, field, field_idx, nullptr);
}

inline bool Class::CanAccessResolvedMethod(Class* access_to, ArtMethod* method,
                                           DexCache* dex_cache, uint32_t method_idx) {
  return ResolvedMethodAccessTest<false, false, kStatic>(access_to, method, method_idx, dex_cache);
}

template <InvokeType throw_invoke_type>
inline bool Class::CheckResolvedMethodAccess(Class* access_to, ArtMethod* method,
                                             uint32_t method_idx) {
  return ResolvedMethodAccessTest<true, true, throw_invoke_type>(access_to, method, method_idx,
                                                                 nullptr);
}

inline bool Class::IsSubClass(Class* klass) {
  DCHECK(!IsInterface()) << PrettyClass(this);
  DCHECK(!IsArrayClass()) << PrettyClass(this);
  Class* current = this;
  do {
    if (current == klass) {
      return true;
    }
    current = current->GetSuperClass();
  } while (current != nullptr);
  return false;
}

inline ArtMethod* Class::FindVirtualMethodForInterface(ArtMethod* method, size_t pointer_size) {
  Class* declaring_class = method->GetDeclaringClass();
  DCHECK(declaring_class != nullptr) << PrettyClass(this);
  DCHECK(declaring_class->IsInterface()) << PrettyMethod(method);
  // TODO cache to improve lookup speed
  const int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    if (iftable->GetInterface(i) == declaring_class) {
      return iftable->GetMethodArray(i)->GetElementPtrSize<ArtMethod*>(
          method->GetMethodIndex(), pointer_size);
    }
  }
  return nullptr;
}

inline ArtMethod* Class::FindVirtualMethodForVirtual(ArtMethod* method, size_t pointer_size) {
  DCHECK(!method->GetDeclaringClass()->IsInterface() || method->IsMiranda());
  // The argument method may from a super class.
  // Use the index to a potentially overridden one for this instance's class.
  return GetVTableEntry(method->GetMethodIndex(), pointer_size);
}

inline ArtMethod* Class::FindVirtualMethodForSuper(ArtMethod* method, size_t pointer_size) {
  DCHECK(!method->GetDeclaringClass()->IsInterface());
  return GetSuperClass()->GetVTableEntry(method->GetMethodIndex(), pointer_size);
}

inline ArtMethod* Class::FindVirtualMethodForVirtualOrInterface(ArtMethod* method,
                                                                size_t pointer_size) {
  if (method->IsDirect()) {
    return method;
  }
  if (method->GetDeclaringClass()->IsInterface() && !method->IsMiranda()) {
    return FindVirtualMethodForInterface(method, pointer_size);
  }
  return FindVirtualMethodForVirtual(method, pointer_size);
}

inline IfTable* Class::GetIfTable() {
  return GetFieldObject<IfTable>(OFFSET_OF_OBJECT_MEMBER(Class, iftable_));
}

inline int32_t Class::GetIfTableCount() {
  IfTable* iftable = GetIfTable();
  if (iftable == nullptr) {
    return 0;
  }
  return iftable->Count();
}

inline void Class::SetIfTable(IfTable* new_iftable) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, iftable_), new_iftable);
}

inline ArtField* Class::GetIFields() {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldPtr<ArtField*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
}

inline MemberOffset Class::GetFirstReferenceInstanceFieldOffset() {
  Class* super_class = GetSuperClass();
  return (super_class != nullptr)
      ? MemberOffset(RoundUp(super_class->GetObjectSize(),
                             sizeof(mirror::HeapReference<mirror::Object>)))
      : ClassOffset();
}

inline MemberOffset Class::GetFirstReferenceStaticFieldOffset(size_t pointer_size) {
  DCHECK(IsResolved());
  uint32_t base = sizeof(mirror::Class);  // Static fields come after the class.
  if (ShouldHaveEmbeddedImtAndVTable()) {
    // Static fields come after the embedded tables.
    base = mirror::Class::ComputeClassSize(
        true, GetEmbeddedVTableLength(), 0, 0, 0, 0, 0, pointer_size);
  }
  return MemberOffset(base);
}

inline MemberOffset Class::GetFirstReferenceStaticFieldOffsetDuringLinking(size_t pointer_size) {
  DCHECK(IsLoaded());
  uint32_t base = sizeof(mirror::Class);  // Static fields come after the class.
  if (ShouldHaveEmbeddedImtAndVTable()) {
    // Static fields come after the embedded tables.
    base = mirror::Class::ComputeClassSize(true, GetVTableDuringLinking()->GetLength(),
                                           0, 0, 0, 0, 0, pointer_size);
  }
  return MemberOffset(base);
}

inline void Class::SetIFields(ArtField* new_ifields) {
  DCHECK(GetIFieldsUnchecked() == nullptr);
  return SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields);
}

inline void Class::SetIFieldsUnchecked(ArtField* new_ifields) {
  SetFieldPtr<false, true, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields);
}

inline ArtField* Class::GetSFieldsUnchecked() {
  return GetFieldPtr<ArtField*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_));
}

inline ArtField* Class::GetIFieldsUnchecked() {
  return GetFieldPtr<ArtField*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
}

inline ArtField* Class::GetSFields() {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetSFieldsUnchecked();
}

inline void Class::SetSFields(ArtField* new_sfields) {
  DCHECK((IsRetired() && new_sfields == nullptr) ||
         GetFieldPtr<ArtField*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_)) == nullptr);
  SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields);
}

inline void Class::SetSFieldsUnchecked(ArtField* new_sfields) {
  SetFieldPtr<false, true, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields);
}

inline ArtField* Class::GetStaticField(uint32_t i) {
  DCHECK_LT(i, NumStaticFields());
  return &GetSFields()[i];
}

inline ArtField* Class::GetInstanceField(uint32_t i) {
  DCHECK_LT(i, NumInstanceFields());
  return &GetIFields()[i];
}

template<VerifyObjectFlags kVerifyFlags>
inline uint32_t Class::GetReferenceInstanceOffsets() {
  DCHECK(IsResolved<kVerifyFlags>() || IsErroneous<kVerifyFlags>());
  return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, reference_instance_offsets_));
}

inline void Class::SetClinitThreadId(pid_t new_clinit_thread_id) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetField32<true>(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_), new_clinit_thread_id);
  } else {
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_), new_clinit_thread_id);
  }
}

inline void Class::SetVerifyErrorClass(Class* klass) {
  CHECK(klass != nullptr) << PrettyClass(this);
  if (Runtime::Current()->IsActiveTransaction()) {
    SetFieldObject<true>(OFFSET_OF_OBJECT_MEMBER(Class, verify_error_class_), klass);
  } else {
    SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, verify_error_class_), klass);
  }
}

template<VerifyObjectFlags kVerifyFlags>
inline uint32_t Class::GetAccessFlags() {
  // Check class is loaded/retired or this is java.lang.String that has a
  // circularity issue during loading the names of its members
  DCHECK(IsIdxLoaded<kVerifyFlags>() || IsRetired<kVerifyFlags>() ||
         IsErroneous<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>() ||
         this == String::GetJavaLangString())
      << "IsIdxLoaded=" << IsIdxLoaded<kVerifyFlags>()
      << " IsRetired=" << IsRetired<kVerifyFlags>()
      << " IsErroneous=" <<
          IsErroneous<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>()
      << " IsString=" << (this == String::GetJavaLangString())
      << " descriptor=" << PrettyDescriptor(this);
  return GetField32<kVerifyFlags>(AccessFlagsOffset());
}

inline String* Class::GetName() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(Class, name_));
}
inline void Class::SetName(String* name) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetFieldObject<true>(OFFSET_OF_OBJECT_MEMBER(Class, name_), name);
  } else {
    SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, name_), name);
  }
}

template<VerifyObjectFlags kVerifyFlags>
inline Primitive::Type Class::GetPrimitiveType() {
  DCHECK_EQ(sizeof(Primitive::Type), sizeof(int32_t));
  int32_t v32 = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_));
  Primitive::Type type = static_cast<Primitive::Type>(v32 & 0xFFFF);
  DCHECK_EQ(static_cast<size_t>(v32 >> 16), Primitive::ComponentSizeShift(type));
  return type;
}

template<VerifyObjectFlags kVerifyFlags>
inline size_t Class::GetPrimitiveTypeSizeShift() {
  DCHECK_EQ(sizeof(Primitive::Type), sizeof(int32_t));
  int32_t v32 = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_));
  size_t size_shift = static_cast<Primitive::Type>(v32 >> 16);
  DCHECK_EQ(size_shift, Primitive::ComponentSizeShift(static_cast<Primitive::Type>(v32 & 0xFFFF)));
  return size_shift;
}

inline void Class::CheckObjectAlloc() {
  DCHECK(!IsArrayClass())
      << PrettyClass(this)
      << "A array shouldn't be allocated through this "
      << "as it requires a pre-fence visitor that sets the class size.";
  DCHECK(!IsClassClass())
      << PrettyClass(this)
      << "A class object shouldn't be allocated through this "
      << "as it requires a pre-fence visitor that sets the class size.";
  DCHECK(!IsStringClass())
      << PrettyClass(this)
      << "A string shouldn't be allocated through this "
      << "as it requires a pre-fence visitor that sets the class size.";
  DCHECK(IsInstantiable()) << PrettyClass(this);
  // TODO: decide whether we want this check. It currently fails during bootstrap.
  // DCHECK(!Runtime::Current()->IsStarted() || IsInitializing()) << PrettyClass(this);
  DCHECK_GE(this->object_size_, sizeof(Object));
}

template<bool kIsInstrumented, bool kCheckAddFinalizer>
inline Object* Class::Alloc(Thread* self, gc::AllocatorType allocator_type) {
  CheckObjectAlloc();
  gc::Heap* heap = Runtime::Current()->GetHeap();
  const bool add_finalizer = kCheckAddFinalizer && IsFinalizable();
  if (!kCheckAddFinalizer) {
    DCHECK(!IsFinalizable());
  }
  mirror::Object* obj =
      heap->AllocObjectWithAllocator<kIsInstrumented, false>(self, this, this->object_size_,
                                                             allocator_type, VoidFunctor());
  if (add_finalizer && LIKELY(obj != nullptr)) {
    heap->AddFinalizerReference(self, &obj);
    if (UNLIKELY(self->IsExceptionPending())) {
      // Failed to allocate finalizer reference, it means that the whole allocation failed.
      obj = nullptr;
    }
  }
  return obj;
}

inline Object* Class::AllocObject(Thread* self) {
  return Alloc<true>(self, Runtime::Current()->GetHeap()->GetCurrentAllocator());
}

inline Object* Class::AllocNonMovableObject(Thread* self) {
  return Alloc<true>(self, Runtime::Current()->GetHeap()->GetCurrentNonMovingAllocator());
}

inline uint32_t Class::ComputeClassSize(bool has_embedded_tables,
                                        uint32_t num_vtable_entries,
                                        uint32_t num_8bit_static_fields,
                                        uint32_t num_16bit_static_fields,
                                        uint32_t num_32bit_static_fields,
                                        uint32_t num_64bit_static_fields,
                                        uint32_t num_ref_static_fields,
                                        size_t pointer_size) {
  // Space used by java.lang.Class and its instance fields.
  uint32_t size = sizeof(Class);
  // Space used by embedded tables.
  if (has_embedded_tables) {
    const uint32_t embedded_imt_size = kImtSize * ImTableEntrySize(pointer_size);
    const uint32_t embedded_vtable_size = num_vtable_entries * VTableEntrySize(pointer_size);
    size = RoundUp(size + sizeof(uint32_t) /* embedded vtable len */, pointer_size) +
        embedded_imt_size + embedded_vtable_size;
  }

  // Space used by reference statics.
  size += num_ref_static_fields * sizeof(HeapReference<Object>);
  if (!IsAligned<8>(size) && num_64bit_static_fields > 0) {
    uint32_t gap = 8 - (size & 0x7);
    size += gap;  // will be padded
    // Shuffle 4-byte fields forward.
    while (gap >= sizeof(uint32_t) && num_32bit_static_fields != 0) {
      --num_32bit_static_fields;
      gap -= sizeof(uint32_t);
    }
    // Shuffle 2-byte fields forward.
    while (gap >= sizeof(uint16_t) && num_16bit_static_fields != 0) {
      --num_16bit_static_fields;
      gap -= sizeof(uint16_t);
    }
    // Shuffle byte fields forward.
    while (gap >= sizeof(uint8_t) && num_8bit_static_fields != 0) {
      --num_8bit_static_fields;
      gap -= sizeof(uint8_t);
    }
  }
  // Guaranteed to be at least 4 byte aligned. No need for further alignments.
  // Space used for primitive static fields.
  size += num_8bit_static_fields * sizeof(uint8_t) + num_16bit_static_fields * sizeof(uint16_t) +
      num_32bit_static_fields * sizeof(uint32_t) + num_64bit_static_fields * sizeof(uint64_t);
  return size;
}

template <bool kVisitClass, typename Visitor>
inline void Class::VisitReferences(mirror::Class* klass, const Visitor& visitor) {
  VisitInstanceFieldsReferences<kVisitClass>(klass, visitor);
  // Right after a class is allocated, but not yet loaded
  // (kStatusNotReady, see ClassLinkder::LoadClass()), GC may find it
  // and scan it. IsTemp() may call Class::GetAccessFlags() but may
  // fail in the DCHECK in Class::GetAccessFlags() because the class
  // status is kStatusNotReady. To avoid it, rely on IsResolved()
  // only. This is fine because a temp class never goes into the
  // kStatusResolved state.
  if (IsResolved()) {
    // Temp classes don't ever populate imt/vtable or static fields and they are not even
    // allocated with the right size for those. Also, unresolved classes don't have fields
    // linked yet.
    VisitStaticFieldsReferences<kVisitClass>(this, visitor);
  }
}

template<ReadBarrierOption kReadBarrierOption>
inline bool Class::IsReferenceClass() const {
  return this == Reference::GetJavaLangRefReference<kReadBarrierOption>();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Class::IsClassClass() {
  Class* java_lang_Class = GetClass<kVerifyFlags, kReadBarrierOption>()->
      template GetClass<kVerifyFlags, kReadBarrierOption>();
  return this == java_lang_Class;
}

inline const DexFile& Class::GetDexFile() {
  return *GetDexCache()->GetDexFile();
}

inline bool Class::DescriptorEquals(const char* match) {
  if (IsArrayClass()) {
    return match[0] == '[' && GetComponentType()->DescriptorEquals(match + 1);
  } else if (IsPrimitive()) {
    return strcmp(Primitive::Descriptor(GetPrimitiveType()), match) == 0;
  } else if (IsProxyClass()) {
    return ProxyDescriptorEquals(match);
  } else {
    const DexFile& dex_file = GetDexFile();
    const DexFile::TypeId& type_id = dex_file.GetTypeId(GetClassDef()->class_idx_);
    return strcmp(dex_file.GetTypeDescriptor(type_id), match) == 0;
  }
}

inline void Class::AssertInitializedOrInitializingInThread(Thread* self) {
  if (kIsDebugBuild && !IsInitialized()) {
    CHECK(IsInitializing()) << PrettyClass(this) << " is not initializing: " << GetStatus();
    CHECK_EQ(GetClinitThreadId(), self->GetTid()) << PrettyClass(this)
                                                  << " is initializing in a different thread";
  }
}

inline ObjectArray<Class>* Class::GetInterfaces() {
  CHECK(IsProxyClass());
  // First static field.
  auto* field = GetStaticField(0);
  DCHECK_STREQ(field->GetName(), "interfaces");
  MemberOffset field_offset = field->GetOffset();
  return GetFieldObject<ObjectArray<Class>>(field_offset);
}

inline ObjectArray<ObjectArray<Class>>* Class::GetThrows() {
  CHECK(IsProxyClass());
  // Second static field.
  auto* field = GetStaticField(1);
  DCHECK_STREQ(field->GetName(), "throws");
  MemberOffset field_offset = field->GetOffset();
  return GetFieldObject<ObjectArray<ObjectArray<Class>>>(field_offset);
}

inline MemberOffset Class::GetDisableIntrinsicFlagOffset() {
  CHECK(IsReferenceClass());
  // First static field
  auto* field = GetStaticField(0);
  DCHECK_STREQ(field->GetName(), "disableIntrinsic");
  return field->GetOffset();
}

inline MemberOffset Class::GetSlowPathFlagOffset() {
  CHECK(IsReferenceClass());
  // Second static field
  auto* field = GetStaticField(1);
  DCHECK_STREQ(field->GetName(), "slowPathEnabled");
  return field->GetOffset();
}

inline bool Class::GetSlowPathEnabled() {
  return GetFieldBoolean(GetSlowPathFlagOffset());
}

inline void Class::SetSlowPath(bool enabled) {
  SetFieldBoolean<false, false>(GetSlowPathFlagOffset(), enabled);
}

inline void Class::InitializeClassVisitor::operator()(
    mirror::Object* obj, size_t usable_size) const {
  DCHECK_LE(class_size_, usable_size);
  // Avoid AsClass as object is not yet in live bitmap or allocation stack.
  mirror::Class* klass = down_cast<mirror::Class*>(obj);
  // DCHECK(klass->IsClass());
  klass->SetClassSize(class_size_);
  klass->SetPrimitiveType(Primitive::kPrimNot);  // Default to not being primitive.
  klass->SetDexClassDefIndex(DexFile::kDexNoIndex16);  // Default to no valid class def index.
  klass->SetDexTypeIndex(DexFile::kDexNoIndex16);  // Default to no valid type index.
}

inline void Class::SetAccessFlags(uint32_t new_access_flags) {
  // Called inside a transaction when setting pre-verified flag during boot image compilation.
  if (Runtime::Current()->IsActiveTransaction()) {
    SetField32<true>(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_), new_access_flags);
  } else {
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_), new_access_flags);
  }
}

inline uint32_t Class::NumDirectInterfaces() {
  if (IsPrimitive()) {
    return 0;
  } else if (IsArrayClass()) {
    return 2;
  } else if (IsProxyClass()) {
    mirror::ObjectArray<mirror::Class>* interfaces = GetInterfaces();
    return interfaces != nullptr ? interfaces->GetLength() : 0;
  } else {
    const DexFile::TypeList* interfaces = GetInterfaceTypeList();
    if (interfaces == nullptr) {
      return 0;
    } else {
      return interfaces->Size();
    }
  }
}

inline void Class::SetDexCacheStrings(ObjectArray<String>* new_dex_cache_strings) {
  SetFieldObject<false>(DexCacheStringsOffset(), new_dex_cache_strings);
}

inline ObjectArray<String>* Class::GetDexCacheStrings() {
  return GetFieldObject<ObjectArray<String>>(DexCacheStringsOffset());
}

template<class Visitor>
void mirror::Class::VisitNativeRoots(Visitor& visitor, size_t pointer_size) {
  ArtField* const sfields = GetSFieldsUnchecked();
  // Since we visit class roots while we may be writing these fields, check against null.
  if (sfields != nullptr) {
    for (size_t i = 0, count = NumStaticFields(); i < count; ++i) {
      auto* f = &sfields[i];
      if (kIsDebugBuild && IsResolved()) {
        CHECK_EQ(f->GetDeclaringClass(), this) << GetStatus();
      }
      f->VisitRoots(visitor);
    }
  }
  ArtField* const ifields = GetIFieldsUnchecked();
  if (ifields != nullptr) {
    for (size_t i = 0, count = NumInstanceFields(); i < count; ++i) {
      auto* f = &ifields[i];
      if (kIsDebugBuild && IsResolved()) {
        CHECK_EQ(f->GetDeclaringClass(), this) << GetStatus();
      }
      f->VisitRoots(visitor);
    }
  }
  for (auto& m : GetDirectMethods(pointer_size)) {
    m.VisitRoots(visitor);
  }
  for (auto& m : GetVirtualMethods(pointer_size)) {
    m.VisitRoots(visitor);
  }
}

inline StrideIterator<ArtMethod> Class::DirectMethodsBegin(size_t pointer_size)  {
  CheckPointerSize(pointer_size);
  auto* methods = GetDirectMethodsPtrUnchecked();
  auto stride = ArtMethod::ObjectSize(pointer_size);
  return StrideIterator<ArtMethod>(reinterpret_cast<uintptr_t>(methods), stride);
}

inline StrideIterator<ArtMethod> Class::DirectMethodsEnd(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  auto* methods = GetDirectMethodsPtrUnchecked();
  auto stride = ArtMethod::ObjectSize(pointer_size);
  auto count = NumDirectMethods();
  return StrideIterator<ArtMethod>(reinterpret_cast<uintptr_t>(methods) + stride * count, stride);
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetDirectMethods(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return MakeIterationRange(DirectMethodsBegin(pointer_size), DirectMethodsEnd(pointer_size));
}

inline StrideIterator<ArtMethod> Class::VirtualMethodsBegin(size_t pointer_size)  {
  CheckPointerSize(pointer_size);
  auto* methods = GetVirtualMethodsPtrUnchecked();
  auto stride = ArtMethod::ObjectSize(pointer_size);
  return StrideIterator<ArtMethod>(reinterpret_cast<uintptr_t>(methods), stride);
}

inline StrideIterator<ArtMethod> Class::VirtualMethodsEnd(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  auto* methods = GetVirtualMethodsPtrUnchecked();
  auto stride = ArtMethod::ObjectSize(pointer_size);
  auto count = NumVirtualMethods();
  return StrideIterator<ArtMethod>(reinterpret_cast<uintptr_t>(methods) + stride * count, stride);
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetVirtualMethods(size_t pointer_size) {
  return MakeIterationRange(VirtualMethodsBegin(pointer_size), VirtualMethodsEnd(pointer_size));
}

inline MemberOffset Class::EmbeddedImTableOffset(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  // Round up since we want the embedded imt and vtable to be pointer size aligned in case 64 bits.
  // Add 32 bits for embedded vtable length.
  return MemberOffset(
      RoundUp(EmbeddedVTableLengthOffset().Uint32Value() + sizeof(uint32_t), pointer_size));
}

inline MemberOffset Class::EmbeddedVTableOffset(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return MemberOffset(EmbeddedImTableOffset(pointer_size).Uint32Value() +
                      kImtSize * ImTableEntrySize(pointer_size));
}

inline void Class::CheckPointerSize(size_t pointer_size) {
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  DCHECK_EQ(pointer_size, Runtime::Current()->GetClassLinker()->GetImagePointerSize());
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_INL_H_
