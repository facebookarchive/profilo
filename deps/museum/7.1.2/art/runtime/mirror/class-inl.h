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
#include "base/array_slice.h"
#include "base/length_prefixed_array.h"
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

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Class* Class::GetSuperClass() {
  // Can only get super class for loaded classes (hack for when runtime is
  // initializing)
  DCHECK(IsLoaded<kVerifyFlags>() ||
         IsErroneous<kVerifyFlags>() ||
         !Runtime::Current()->IsStarted()) << IsLoaded();
  return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, super_class_));
}

inline ClassLoader* Class::GetClassLoader() {
  return GetFieldObject<ClassLoader>(OFFSET_OF_OBJECT_MEMBER(Class, class_loader_));
}

template<VerifyObjectFlags kVerifyFlags>
inline DexCache* Class::GetDexCache() {
  return GetFieldObject<DexCache, kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_));
}

inline uint32_t Class::GetCopiedMethodsStartOffset() {
  return GetFieldShort(OFFSET_OF_OBJECT_MEMBER(Class, copied_methods_offset_));
}

inline uint32_t Class::GetDirectMethodsStartOffset() {
  return 0;
}

inline uint32_t Class::GetVirtualMethodsStartOffset() {
  return GetFieldShort(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_offset_));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetDirectMethodsSlice(size_t pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  return GetDirectMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDirectMethodsSliceUnchecked(size_t pointer_size) {
  return ArraySlice<ArtMethod>(GetMethodsPtr(),
                               GetDirectMethodsStartOffset(),
                               GetVirtualMethodsStartOffset(),
                               ArtMethod::Size(pointer_size),
                               ArtMethod::Alignment(pointer_size));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetDeclaredMethodsSlice(size_t pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  return GetDeclaredMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDeclaredMethodsSliceUnchecked(size_t pointer_size) {
  return ArraySlice<ArtMethod>(GetMethodsPtr(),
                               GetDirectMethodsStartOffset(),
                               GetCopiedMethodsStartOffset(),
                               ArtMethod::Size(pointer_size),
                               ArtMethod::Alignment(pointer_size));
}
template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetDeclaredVirtualMethodsSlice(size_t pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  return GetDeclaredVirtualMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDeclaredVirtualMethodsSliceUnchecked(size_t pointer_size) {
  return ArraySlice<ArtMethod>(GetMethodsPtr(),
                               GetVirtualMethodsStartOffset(),
                               GetCopiedMethodsStartOffset(),
                               ArtMethod::Size(pointer_size),
                               ArtMethod::Alignment(pointer_size));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetVirtualMethodsSlice(size_t pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  return GetVirtualMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetVirtualMethodsSliceUnchecked(size_t pointer_size) {
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return ArraySlice<ArtMethod>(methods,
                               GetVirtualMethodsStartOffset(),
                               NumMethods(),
                               ArtMethod::Size(pointer_size),
                               ArtMethod::Alignment(pointer_size));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetCopiedMethodsSlice(size_t pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  return GetCopiedMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetCopiedMethodsSliceUnchecked(size_t pointer_size) {
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return ArraySlice<ArtMethod>(methods,
                               GetCopiedMethodsStartOffset(),
                               NumMethods(),
                               ArtMethod::Size(pointer_size),
                               ArtMethod::Alignment(pointer_size));
}

inline LengthPrefixedArray<ArtMethod>* Class::GetMethodsPtr() {
  return reinterpret_cast<LengthPrefixedArray<ArtMethod>*>(
      static_cast<uintptr_t>(GetField64(OFFSET_OF_OBJECT_MEMBER(Class, methods_))));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetMethodsSlice(size_t pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return ArraySlice<ArtMethod>(methods,
                               0,
                               NumMethods(),
                               ArtMethod::Size(pointer_size),
                               ArtMethod::Alignment(pointer_size));
}


inline uint32_t Class::NumMethods() {
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return (methods == nullptr) ? 0 : methods->size();
}

inline ArtMethod* Class::GetDirectMethodUnchecked(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return &GetDirectMethodsSliceUnchecked(pointer_size).At(i);
}

inline ArtMethod* Class::GetDirectMethod(size_t i, size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return &GetDirectMethodsSlice(pointer_size).At(i);
}

inline void Class::SetMethodsPtr(LengthPrefixedArray<ArtMethod>* new_methods,
                                 uint32_t num_direct,
                                 uint32_t num_virtual) {
  DCHECK(GetMethodsPtr() == nullptr);
  SetMethodsPtrUnchecked(new_methods, num_direct, num_virtual);
}


inline void Class::SetMethodsPtrUnchecked(LengthPrefixedArray<ArtMethod>* new_methods,
                                          uint32_t num_direct,
                                          uint32_t num_virtual) {
  DCHECK_LE(num_direct + num_virtual, (new_methods == nullptr) ? 0 : new_methods->size());
  SetMethodsPtrInternal(new_methods);
  SetFieldShort<false>(OFFSET_OF_OBJECT_MEMBER(Class, copied_methods_offset_),
                    dchecked_integral_cast<uint16_t>(num_direct + num_virtual));
  SetFieldShort<false>(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_offset_),
                       dchecked_integral_cast<uint16_t>(num_direct));
}

inline void Class::SetMethodsPtrInternal(LengthPrefixedArray<ArtMethod>* new_methods) {
  SetField64<false>(OFFSET_OF_OBJECT_MEMBER(Class, methods_),
                    static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new_methods)));
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
  return &GetVirtualMethodsSliceUnchecked(pointer_size).At(i);
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline PointerArray* Class::GetVTable() {
  DCHECK(IsResolved<kVerifyFlags>() || IsErroneous<kVerifyFlags>());
  return GetFieldObject<PointerArray, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, vtable_));
}

inline PointerArray* Class::GetVTableDuringLinking() {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<PointerArray>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_));
}

inline void Class::SetVTable(PointerArray* new_vtable) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_), new_vtable);
}

inline bool Class::HasVTable() {
  return GetVTable() != nullptr || ShouldHaveEmbeddedVTable();
}

inline int32_t Class::GetVTableLength() {
  if (ShouldHaveEmbeddedVTable()) {
    return GetEmbeddedVTableLength();
  }
  return GetVTable() != nullptr ? GetVTable()->GetLength() : 0;
}

inline ArtMethod* Class::GetVTableEntry(uint32_t i, size_t pointer_size) {
  if (ShouldHaveEmbeddedVTable()) {
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

inline ImTable* Class::GetImt(size_t pointer_size) {
  return GetFieldPtrWithSize<ImTable*>(MemberOffset(ImtPtrOffset(pointer_size)), pointer_size);
}

inline void Class::SetImt(ImTable* imt, size_t pointer_size) {
  return SetFieldPtrWithSize<false>(MemberOffset(ImtPtrOffset(pointer_size)), imt, pointer_size);
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
    // The referenced class has already been resolved with the field, but may not be in the dex
    // cache. Using ResolveType here without handles in the caller should be safe since there
    // should be no thread suspension due to the class being resolved.
    // TODO: Clean this up to use handles in the caller.
    Class* dex_access_to;
    {
      StackHandleScope<2> hs(Thread::Current());
      Handle<mirror::DexCache> h_dex_cache(hs.NewHandle(referrer_dex_cache));
      Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(access_to->GetClassLoader()));
      dex_access_to = Runtime::Current()->GetClassLinker()->ResolveType(
          *referrer_dex_cache->GetDexFile(),
          class_idx,
          h_dex_cache,
          h_class_loader);
    }
    DCHECK(dex_access_to != nullptr);
    if (UNLIKELY(!this->CanAccess(dex_access_to))) {
      if (throw_on_failure) {
        ThrowIllegalAccessErrorClass(this, dex_access_to);
      }
      return false;
    }
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
    // The referenced class has already been resolved with the method, but may not be in the dex
    // cache. Using ResolveType here without handles in the caller should be safe since there
    // should be no thread suspension due to the class being resolved.
    // TODO: Clean this up to use handles in the caller.
    Class* dex_access_to;
    {
      StackHandleScope<2> hs(Thread::Current());
      Handle<mirror::DexCache> h_dex_cache(hs.NewHandle(referrer_dex_cache));
      Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(access_to->GetClassLoader()));
      dex_access_to = Runtime::Current()->GetClassLinker()->ResolveType(
          *referrer_dex_cache->GetDexFile(),
          class_idx,
          h_dex_cache,
          h_class_loader);
    }
    DCHECK(dex_access_to != nullptr);
    if (UNLIKELY(!this->CanAccess(dex_access_to))) {
      if (throw_on_failure) {
        ThrowIllegalAccessErrorClassForMethodDispatch(this, dex_access_to,
                                                      method, throw_invoke_type);
      }
      return false;
    }
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
  // Only miranda or default methods may come from interfaces and be used as a virtual.
  DCHECK(!method->GetDeclaringClass()->IsInterface() || method->IsDefault() || method->IsMiranda());
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
  if (method->GetDeclaringClass()->IsInterface() && !method->IsCopied()) {
    return FindVirtualMethodForInterface(method, pointer_size);
  }
  return FindVirtualMethodForVirtual(method, pointer_size);
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline IfTable* Class::GetIfTable() {
  return GetFieldObject<IfTable, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, iftable_));
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

inline LengthPrefixedArray<ArtField>* Class::GetIFieldsPtr() {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetFieldPtr<LengthPrefixedArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline MemberOffset Class::GetFirstReferenceInstanceFieldOffset() {
  Class* super_class = GetSuperClass<kVerifyFlags, kReadBarrierOption>();
  return (super_class != nullptr)
      ? MemberOffset(RoundUp(super_class->GetObjectSize(),
                             sizeof(mirror::HeapReference<mirror::Object>)))
      : ClassOffset();
}

template <VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline MemberOffset Class::GetFirstReferenceStaticFieldOffset(size_t pointer_size) {
  DCHECK(IsResolved());
  uint32_t base = sizeof(mirror::Class);  // Static fields come after the class.
  if (ShouldHaveEmbeddedVTable<kVerifyFlags, kReadBarrierOption>()) {
    // Static fields come after the embedded tables.
    base = mirror::Class::ComputeClassSize(
        true, GetEmbeddedVTableLength(), 0, 0, 0, 0, 0, pointer_size);
  }
  return MemberOffset(base);
}

inline MemberOffset Class::GetFirstReferenceStaticFieldOffsetDuringLinking(size_t pointer_size) {
  DCHECK(IsLoaded());
  uint32_t base = sizeof(mirror::Class);  // Static fields come after the class.
  if (ShouldHaveEmbeddedVTable()) {
    // Static fields come after the embedded tables.
    base = mirror::Class::ComputeClassSize(true, GetVTableDuringLinking()->GetLength(),
                                           0, 0, 0, 0, 0, pointer_size);
  }
  return MemberOffset(base);
}

inline void Class::SetIFieldsPtr(LengthPrefixedArray<ArtField>* new_ifields) {
  DCHECK(GetIFieldsPtrUnchecked() == nullptr);
  return SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields);
}

inline void Class::SetIFieldsPtrUnchecked(LengthPrefixedArray<ArtField>* new_ifields) {
  SetFieldPtr<false, true, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields);
}

inline LengthPrefixedArray<ArtField>* Class::GetSFieldsPtrUnchecked() {
  return GetFieldPtr<LengthPrefixedArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_));
}

inline LengthPrefixedArray<ArtField>* Class::GetIFieldsPtrUnchecked() {
  return GetFieldPtr<LengthPrefixedArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
}

inline LengthPrefixedArray<ArtField>* Class::GetSFieldsPtr() {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetSFieldsPtrUnchecked();
}

inline void Class::SetSFieldsPtr(LengthPrefixedArray<ArtField>* new_sfields) {
  DCHECK((IsRetired() && new_sfields == nullptr) ||
         GetFieldPtr<ArtField*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_)) == nullptr);
  SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields);
}

inline void Class::SetSFieldsPtrUnchecked(LengthPrefixedArray<ArtField>* new_sfields) {
  SetFieldPtr<false, true, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields);
}

inline ArtField* Class::GetStaticField(uint32_t i) {
  return &GetSFieldsPtr()->At(i);
}

inline ArtField* Class::GetInstanceField(uint32_t i) {
  return &GetIFieldsPtr()->At(i);
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
      << " status= " << GetStatus<kVerifyFlags>()
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
  static_assert(sizeof(Primitive::Type) == sizeof(int32_t),
                "art::Primitive::Type and int32_t have different sizes.");
  int32_t v32 = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_));
  Primitive::Type type = static_cast<Primitive::Type>(v32 & 0xFFFF);
  DCHECK_EQ(static_cast<size_t>(v32 >> 16), Primitive::ComponentSizeShift(type));
  return type;
}

template<VerifyObjectFlags kVerifyFlags>
inline size_t Class::GetPrimitiveTypeSizeShift() {
  static_assert(sizeof(Primitive::Type) == sizeof(int32_t),
                "art::Primitive::Type and int32_t have different sizes.");
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

inline uint32_t Class::ComputeClassSize(bool has_embedded_vtable,
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
  if (has_embedded_vtable) {
    size = RoundUp(size + sizeof(uint32_t), pointer_size);
    size += pointer_size;  // size of pointer to IMT
    size += num_vtable_entries * VTableEntrySize(pointer_size);
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

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void Class::VisitReferences(mirror::Class* klass, const Visitor& visitor) {
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
  // Right after a class is allocated, but not yet loaded
  // (kStatusNotReady, see ClassLinker::LoadClass()), GC may find it
  // and scan it. IsTemp() may call Class::GetAccessFlags() but may
  // fail in the DCHECK in Class::GetAccessFlags() because the class
  // status is kStatusNotReady. To avoid it, rely on IsResolved()
  // only. This is fine because a temp class never goes into the
  // kStatusResolved state.
  if (IsResolved<kVerifyFlags>()) {
    // Temp classes don't ever populate imt/vtable or static fields and they are not even
    // allocated with the right size for those. Also, unresolved classes don't have fields
    // linked yet.
    VisitStaticFieldsReferences<kVerifyFlags, kReadBarrierOption>(this, visitor);
  }
  if (kVisitNativeRoots) {
    // Since this class is reachable, we must also visit the associated roots when we scan it.
    VisitNativeRoots(visitor, Runtime::Current()->GetClassLinker()->GetImagePointerSize());
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
    SetField32<true>(AccessFlagsOffset(), new_access_flags);
  } else {
    SetField32<false>(AccessFlagsOffset(), new_access_flags);
  }
}

inline void Class::SetClassFlags(uint32_t new_flags) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetField32<true>(OFFSET_OF_OBJECT_MEMBER(Class, class_flags_), new_flags);
  } else {
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, class_flags_), new_flags);
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

inline void Class::SetDexCacheStrings(GcRoot<String>* new_dex_cache_strings) {
  SetFieldPtr<false>(DexCacheStringsOffset(), new_dex_cache_strings);
}

inline GcRoot<String>* Class::GetDexCacheStrings() {
  return GetFieldPtr<GcRoot<String>*>(DexCacheStringsOffset());
}

template<class Visitor>
void mirror::Class::VisitNativeRoots(Visitor& visitor, size_t pointer_size) {
  for (ArtField& field : GetSFieldsUnchecked()) {
    // Visit roots first in case the declaring class gets moved.
    field.VisitRoots(visitor);
    if (kIsDebugBuild && IsResolved()) {
      CHECK_EQ(field.GetDeclaringClass(), this) << GetStatus();
    }
  }
  for (ArtField& field : GetIFieldsUnchecked()) {
    // Visit roots first in case the declaring class gets moved.
    field.VisitRoots(visitor);
    if (kIsDebugBuild && IsResolved()) {
      CHECK_EQ(field.GetDeclaringClass(), this) << GetStatus();
    }
  }
  for (ArtMethod& method : GetMethods(pointer_size)) {
    method.VisitRoots(visitor, pointer_size);
  }
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetDirectMethods(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return GetDirectMethodsSliceUnchecked(pointer_size).AsRange();
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetDeclaredMethods(
      size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return GetDeclaredMethodsSliceUnchecked(pointer_size).AsRange();
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetDeclaredVirtualMethods(
      size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return GetDeclaredVirtualMethodsSliceUnchecked(pointer_size).AsRange();
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetVirtualMethods(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return GetVirtualMethodsSliceUnchecked(pointer_size).AsRange();
}

inline IterationRange<StrideIterator<ArtMethod>> Class::GetCopiedMethods(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return GetCopiedMethodsSliceUnchecked(pointer_size).AsRange();
}


inline IterationRange<StrideIterator<ArtMethod>> Class::GetMethods(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return MakeIterationRangeFromLengthPrefixedArray(GetMethodsPtr(),
                                                   ArtMethod::Size(pointer_size),
                                                   ArtMethod::Alignment(pointer_size));
}

inline IterationRange<StrideIterator<ArtField>> Class::GetIFields() {
  return MakeIterationRangeFromLengthPrefixedArray(GetIFieldsPtr());
}

inline IterationRange<StrideIterator<ArtField>> Class::GetSFields() {
  return MakeIterationRangeFromLengthPrefixedArray(GetSFieldsPtr());
}

inline IterationRange<StrideIterator<ArtField>> Class::GetIFieldsUnchecked() {
  return MakeIterationRangeFromLengthPrefixedArray(GetIFieldsPtrUnchecked());
}

inline IterationRange<StrideIterator<ArtField>> Class::GetSFieldsUnchecked() {
  return MakeIterationRangeFromLengthPrefixedArray(GetSFieldsPtrUnchecked());
}

inline MemberOffset Class::EmbeddedVTableOffset(size_t pointer_size) {
  CheckPointerSize(pointer_size);
  return MemberOffset(ImtPtrOffset(pointer_size).Uint32Value() + pointer_size);
}

inline void Class::CheckPointerSize(size_t pointer_size) {
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
  DCHECK_EQ(pointer_size, Runtime::Current()->GetClassLinker()->GetImagePointerSize());
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Class* Class::GetComponentType() {
  return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(ComponentTypeOffset());
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Class::IsArrayClass() {
  return GetComponentType<kVerifyFlags, kReadBarrierOption>() != nullptr;
}

inline bool Class::IsAssignableFrom(Class* src) {
  DCHECK(src != nullptr);
  if (this == src) {
    // Can always assign to things of the same type.
    return true;
  } else if (IsObjectClass()) {
    // Can assign any reference to java.lang.Object.
    return !src->IsPrimitive();
  } else if (IsInterface()) {
    return src->Implements(this);
  } else if (src->IsArrayClass()) {
    return IsAssignableFromArray(src);
  } else {
    return !src->IsInterface() && src->IsSubClass(this);
  }
}

inline uint32_t Class::NumDirectMethods() {
  return GetVirtualMethodsStartOffset();
}

inline uint32_t Class::NumDeclaredVirtualMethods() {
  return GetCopiedMethodsStartOffset() - GetVirtualMethodsStartOffset();
}

inline uint32_t Class::NumVirtualMethods() {
  return NumMethods() - GetVirtualMethodsStartOffset();
}

inline uint32_t Class::NumInstanceFields() {
  LengthPrefixedArray<ArtField>* arr = GetIFieldsPtrUnchecked();
  return arr != nullptr ? arr->size() : 0u;
}

inline uint32_t Class::NumStaticFields() {
  LengthPrefixedArray<ArtField>* arr = GetSFieldsPtrUnchecked();
  return arr != nullptr ? arr->size() : 0u;
}

template <VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void Class::FixupNativePointers(mirror::Class* dest,
                                       size_t pointer_size,
                                       const Visitor& visitor) {
  // Update the field arrays.
  LengthPrefixedArray<ArtField>* const sfields = GetSFieldsPtr();
  LengthPrefixedArray<ArtField>* const new_sfields = visitor(sfields);
  if (sfields != new_sfields) {
    dest->SetSFieldsPtrUnchecked(new_sfields);
  }
  LengthPrefixedArray<ArtField>* const ifields = GetIFieldsPtr();
  LengthPrefixedArray<ArtField>* const new_ifields = visitor(ifields);
  if (ifields != new_ifields) {
    dest->SetIFieldsPtrUnchecked(new_ifields);
  }
  // Update method array.
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  LengthPrefixedArray<ArtMethod>* new_methods = visitor(methods);
  if (methods != new_methods) {
    dest->SetMethodsPtrInternal(new_methods);
  }
  // Update dex cache strings.
  GcRoot<mirror::String>* strings = GetDexCacheStrings();
  GcRoot<mirror::String>* new_strings = visitor(strings);
  if (strings != new_strings) {
    dest->SetDexCacheStrings(new_strings);
  }
  // Fix up embedded tables.
  if (!IsTemp() && ShouldHaveEmbeddedVTable<kVerifyNone, kReadBarrierOption>()) {
    for (int32_t i = 0, count = GetEmbeddedVTableLength(); i < count; ++i) {
      ArtMethod* method = GetEmbeddedVTableEntry(i, pointer_size);
      ArtMethod* new_method = visitor(method);
      if (method != new_method) {
        dest->SetEmbeddedVTableEntryUnchecked(i, new_method, pointer_size);
      }
    }
  }
  if (!IsTemp() && ShouldHaveImt<kVerifyNone, kReadBarrierOption>()) {
    dest->SetImt(visitor(GetImt(pointer_size)), pointer_size);
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_INL_H_
