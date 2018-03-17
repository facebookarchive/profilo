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

#ifndef ART_RUNTIME_ART_METHOD_INL_H_
#define ART_RUNTIME_ART_METHOD_INL_H_

#include "art_method.h"

#include "art_field.h"
#include "base/logging.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "dex_file.h"
#include "dex_file_annotations.h"
#include "dex_file-inl.h"
#include "gc_root-inl.h"
#include "jit/profiling_info.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array.h"
#include "mirror/string.h"
#include "oat.h"
#include "obj_ptr-inl.h"
#include "quick/quick_method_frame_info.h"
#include "read_barrier-inl.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "utils.h"

namespace art {

template <ReadBarrierOption kReadBarrierOption>
inline mirror::Class* ArtMethod::GetDeclaringClassUnchecked() {
  GcRootSource gc_root_source(this);
  return declaring_class_.Read<kReadBarrierOption>(&gc_root_source);
}

template <ReadBarrierOption kReadBarrierOption>
inline mirror::Class* ArtMethod::GetDeclaringClass() {
  mirror::Class* result = GetDeclaringClassUnchecked<kReadBarrierOption>();
  if (kIsDebugBuild) {
    if (!IsRuntimeMethod()) {
      CHECK(result != nullptr) << this;
      if (kCheckDeclaringClassState) {
        if (!(result->IsIdxLoaded() || result->IsErroneous())) {
          LOG(FATAL_WITHOUT_ABORT) << "Class status: " << result->GetStatus();
          LOG(FATAL) << result->PrettyClass();
        }
      }
    } else {
      CHECK(result == nullptr) << this;
    }
  }
  return result;
}

inline void ArtMethod::SetDeclaringClass(ObjPtr<mirror::Class> new_declaring_class) {
  declaring_class_ = GcRoot<mirror::Class>(new_declaring_class);
}

inline bool ArtMethod::CASDeclaringClass(mirror::Class* expected_class,
                                         mirror::Class* desired_class) {
  GcRoot<mirror::Class> expected_root(expected_class);
  GcRoot<mirror::Class> desired_root(desired_class);
  return reinterpret_cast<Atomic<GcRoot<mirror::Class>>*>(&declaring_class_)->
      CompareExchangeStrongSequentiallyConsistent(
          expected_root, desired_root);
}

inline uint16_t ArtMethod::GetMethodIndex() {
  DCHECK(IsRuntimeMethod() || GetDeclaringClass()->IsResolved());
  return method_index_;
}

inline uint16_t ArtMethod::GetMethodIndexDuringLinking() {
  return method_index_;
}

inline uint32_t ArtMethod::GetDexMethodIndex() {
  if (kCheckDeclaringClassState) {
    CHECK(IsRuntimeMethod() || GetDeclaringClass()->IsIdxLoaded() ||
          GetDeclaringClass()->IsErroneous());
  }
  return GetDexMethodIndexUnchecked();
}

inline ArtMethod** ArtMethod::GetDexCacheResolvedMethods(PointerSize pointer_size) {
  return GetNativePointer<ArtMethod**>(DexCacheResolvedMethodsOffset(pointer_size),
                                       pointer_size);
}

inline ArtMethod* ArtMethod::GetDexCacheResolvedMethod(uint16_t method_index,
                                                       PointerSize pointer_size) {
  // NOTE: Unchecked, i.e. not throwing AIOOB. We don't even know the length here
  // without accessing the DexCache and we don't want to do that in release build.
  DCHECK_LT(method_index,
            GetInterfaceMethodIfProxy(pointer_size)->GetDexCache()->NumResolvedMethods());
  ArtMethod* method = mirror::DexCache::GetElementPtrSize(GetDexCacheResolvedMethods(pointer_size),
                                                          method_index,
                                                          pointer_size);
  if (LIKELY(method != nullptr)) {
    auto* declaring_class = method->GetDeclaringClass();
    if (LIKELY(declaring_class == nullptr || !declaring_class->IsErroneous())) {
      return method;
    }
  }
  return nullptr;
}

inline void ArtMethod::SetDexCacheResolvedMethod(uint16_t method_index,
                                                 ArtMethod* new_method,
                                                 PointerSize pointer_size) {
  // NOTE: Unchecked, i.e. not throwing AIOOB. We don't even know the length here
  // without accessing the DexCache and we don't want to do that in release build.
  DCHECK_LT(method_index,
            GetInterfaceMethodIfProxy(pointer_size)->GetDexCache()->NumResolvedMethods());
  DCHECK(new_method == nullptr || new_method->GetDeclaringClass() != nullptr);
  mirror::DexCache::SetElementPtrSize(GetDexCacheResolvedMethods(pointer_size),
                                      method_index,
                                      new_method,
                                      pointer_size);
}

inline bool ArtMethod::HasDexCacheResolvedMethods(PointerSize pointer_size) {
  return GetDexCacheResolvedMethods(pointer_size) != nullptr;
}

inline bool ArtMethod::HasSameDexCacheResolvedMethods(ArtMethod** other_cache,
                                                      PointerSize pointer_size) {
  return GetDexCacheResolvedMethods(pointer_size) == other_cache;
}

inline bool ArtMethod::HasSameDexCacheResolvedMethods(ArtMethod* other, PointerSize pointer_size) {
  return GetDexCacheResolvedMethods(pointer_size) ==
      other->GetDexCacheResolvedMethods(pointer_size);
}

inline mirror::Class* ArtMethod::GetClassFromTypeIndex(dex::TypeIndex type_idx, bool resolve) {
  // TODO: Refactor this function into two functions, Resolve...() and Lookup...()
  // so that we can properly annotate it with no-suspension possible / suspension possible.
  ObjPtr<mirror::DexCache> dex_cache = GetDexCache();
  ObjPtr<mirror::Class> type = dex_cache->GetResolvedType(type_idx);
  if (UNLIKELY(type == nullptr)) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    if (resolve) {
      type = class_linker->ResolveType(type_idx, this);
      CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
    } else {
      type = class_linker->LookupResolvedType(
          *dex_cache->GetDexFile(), type_idx, dex_cache, GetClassLoader());
    }
  }
  return type.Ptr();
}

inline bool ArtMethod::CheckIncompatibleClassChange(InvokeType type) {
  switch (type) {
    case kStatic:
      return !IsStatic();
    case kDirect:
      return !IsDirect() || IsStatic();
    case kVirtual: {
      // We have an error if we are direct or a non-copied (i.e. not part of a real class) interface
      // method.
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || (methods_class->IsInterface() && !IsCopied());
    }
    case kSuper:
      // Constructors and static methods are called with invoke-direct.
      return IsConstructor() || IsStatic();
    case kInterface: {
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || !(methods_class->IsInterface() || methods_class->IsObjectClass());
    }
    default:
      LOG(FATAL) << "Unreachable - invocation type: " << type;
      UNREACHABLE();
  }
}

inline bool ArtMethod::IsCalleeSaveMethod() {
  if (!IsRuntimeMethod()) {
    return false;
  }
  Runtime* runtime = Runtime::Current();
  bool result = false;
  for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
    if (this == runtime->GetCalleeSaveMethod(Runtime::CalleeSaveType(i))) {
      result = true;
      break;
    }
  }
  return result;
}

inline bool ArtMethod::IsResolutionMethod() {
  bool result = this == Runtime::Current()->GetResolutionMethod();
  // Check that if we do think it is phony it looks like the resolution method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline bool ArtMethod::IsImtUnimplementedMethod() {
  bool result = this == Runtime::Current()->GetImtUnimplementedMethod();
  // Check that if we do think it is phony it looks like the imt unimplemented method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline const DexFile* ArtMethod::GetDexFile() {
  // It is safe to avoid the read barrier here since the dex file is constant, so if we read the
  // from-space dex file pointer it will be equal to the to-space copy.
  return GetDexCache<kWithoutReadBarrier>()->GetDexFile();
}

inline const char* ArtMethod::GetDeclaringClassDescriptor() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (UNLIKELY(dex_method_idx == DexFile::kDexNoIndex)) {
    return "<runtime method>";
  }
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodDeclaringClassDescriptor(dex_file->GetMethodId(dex_method_idx));
}

inline const char* ArtMethod::GetShorty() {
  uint32_t unused_length;
  return GetShorty(&unused_length);
}

inline const char* ArtMethod::GetShorty(uint32_t* out_length) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodShorty(dex_file->GetMethodId(GetDexMethodIndex()), out_length);
}

inline const Signature ArtMethod::GetSignature() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (dex_method_idx != DexFile::kDexNoIndex) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    return dex_file->GetMethodSignature(dex_file->GetMethodId(dex_method_idx));
  }
  return Signature::NoSignature();
}

inline const char* ArtMethod::GetName() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (LIKELY(dex_method_idx != DexFile::kDexNoIndex)) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    return dex_file->GetMethodName(dex_file->GetMethodId(dex_method_idx));
  }
  Runtime* const runtime = Runtime::Current();
  if (this == runtime->GetResolutionMethod()) {
    return "<runtime internal resolution method>";
  } else if (this == runtime->GetImtConflictMethod()) {
    return "<runtime internal imt conflict method>";
  } else if (this == runtime->GetCalleeSaveMethod(Runtime::kSaveAllCalleeSaves)) {
    return "<runtime internal callee-save all registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(Runtime::kSaveRefsOnly)) {
    return "<runtime internal callee-save reference registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(Runtime::kSaveRefsAndArgs)) {
    return "<runtime internal callee-save reference and argument registers method>";
  } else {
    return "<unknown runtime internal method>";
  }
}

inline const DexFile::CodeItem* ArtMethod::GetCodeItem() {
  return GetDexFile()->GetCodeItem(GetCodeItemOffset());
}

inline bool ArtMethod::IsResolvedTypeIdx(dex::TypeIndex type_idx) {
  DCHECK(!IsProxyMethod());
  return GetClassFromTypeIndex(type_idx, /* resolve */ false) != nullptr;
}

inline int32_t ArtMethod::GetLineNumFromDexPC(uint32_t dex_pc) {
  DCHECK(!IsProxyMethod());
  if (dex_pc == DexFile::kDexNoIndex) {
    return IsNative() ? -2 : -1;
  }
  return annotations::GetLineNumFromPC(GetDexFile(), this, dex_pc);
}

inline const DexFile::ProtoId& ArtMethod::GetPrototype() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodPrototype(dex_file->GetMethodId(GetDexMethodIndex()));
}

inline const DexFile::TypeList* ArtMethod::GetParameterTypeList() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::ProtoId& proto = dex_file->GetMethodPrototype(
      dex_file->GetMethodId(GetDexMethodIndex()));
  return dex_file->GetProtoParameters(proto);
}

inline const char* ArtMethod::GetDeclaringClassSourceFile() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetSourceFile();
}

inline uint16_t ArtMethod::GetClassDefIndex() {
  DCHECK(!IsProxyMethod());
  if (LIKELY(!IsObsolete())) {
    return GetDeclaringClass()->GetDexClassDefIndex();
  } else {
    return FindObsoleteDexClassDefIndex();
  }
}

inline const DexFile::ClassDef& ArtMethod::GetClassDef() {
  DCHECK(!IsProxyMethod());
  return GetDexFile()->GetClassDef(GetClassDefIndex());
}

inline const char* ArtMethod::GetReturnTypeDescriptor() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(proto_id.return_type_idx_));
}

inline const char* ArtMethod::GetTypeDescriptorFromTypeIdx(dex::TypeIndex type_idx) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(type_idx));
}

inline mirror::ClassLoader* ArtMethod::GetClassLoader() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetClassLoader();
}

template <ReadBarrierOption kReadBarrierOption>
inline mirror::DexCache* ArtMethod::GetDexCache() {
  if (LIKELY(!IsObsolete())) {
    mirror::Class* klass = GetDeclaringClass<kReadBarrierOption>();
    return klass->GetDexCache<kDefaultVerifyFlags, kReadBarrierOption>();
  } else {
    DCHECK(!IsProxyMethod());
    return GetObsoleteDexCache();
  }
}

inline bool ArtMethod::IsProxyMethod() {
  // Avoid read barrier since the from-space version of the class will have the correct proxy class
  // flags since they are constant for the lifetime of the class.
  return GetDeclaringClass<kWithoutReadBarrier>()->IsProxyClass();
}

inline ArtMethod* ArtMethod::GetInterfaceMethodIfProxy(PointerSize pointer_size) {
  if (LIKELY(!IsProxyMethod())) {
    return this;
  }
  ArtMethod* interface_method = mirror::DexCache::GetElementPtrSize(
      GetDexCacheResolvedMethods(pointer_size),
      GetDexMethodIndex(),
      pointer_size);
  DCHECK(interface_method != nullptr);
  DCHECK_EQ(interface_method,
            Runtime::Current()->GetClassLinker()->FindMethodForProxy(GetDeclaringClass(), this));
  return interface_method;
}

inline void ArtMethod::SetDexCacheResolvedMethods(ArtMethod** new_dex_cache_methods,
                                                  PointerSize pointer_size) {
  SetNativePointer(DexCacheResolvedMethodsOffset(pointer_size),
                   new_dex_cache_methods,
                   pointer_size);
}

inline mirror::Class* ArtMethod::GetReturnType(bool resolve) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  dex::TypeIndex return_type_idx = proto_id.return_type_idx_;
  return GetClassFromTypeIndex(return_type_idx, resolve);
}

inline bool ArtMethod::HasSingleImplementation() {
  if (IsFinal() || GetDeclaringClass()->IsFinal()) {
    // We don't set kAccSingleImplementation for these cases since intrinsic
    // can use the flag also.
    return true;
  }
  return (GetAccessFlags() & kAccSingleImplementation) != 0;
}

inline void ArtMethod::SetIntrinsic(uint32_t intrinsic) {
  DCHECK(IsUint<8>(intrinsic));
  // Currently we only do intrinsics for static/final methods or methods of final
  // classes. We don't set kHasSingleImplementation for those methods.
  DCHECK(IsStatic() || IsFinal() || GetDeclaringClass()->IsFinal()) <<
      "Potential conflict with kAccSingleImplementation";
  uint32_t new_value = (GetAccessFlags() & kAccFlagsNotUsedByIntrinsic) |
      kAccIntrinsic |
      (intrinsic << POPCOUNT(kAccFlagsNotUsedByIntrinsic));
  if (kIsDebugBuild) {
    uint32_t java_flags = (GetAccessFlags() & kAccJavaFlagsMask);
    bool is_constructor = IsConstructor();
    bool is_synchronized = IsSynchronized();
    bool skip_access_checks = SkipAccessChecks();
    bool is_fast_native = IsFastNative();
    bool is_copied = IsCopied();
    bool is_miranda = IsMiranda();
    bool is_default = IsDefault();
    bool is_default_conflict = IsDefaultConflicting();
    bool is_compilable = IsCompilable();
    bool must_count_locks = MustCountLocks();
    SetAccessFlags(new_value);
    DCHECK_EQ(java_flags, (GetAccessFlags() & kAccJavaFlagsMask));
    DCHECK_EQ(is_constructor, IsConstructor());
    DCHECK_EQ(is_synchronized, IsSynchronized());
    DCHECK_EQ(skip_access_checks, SkipAccessChecks());
    DCHECK_EQ(is_fast_native, IsFastNative());
    DCHECK_EQ(is_copied, IsCopied());
    DCHECK_EQ(is_miranda, IsMiranda());
    DCHECK_EQ(is_default, IsDefault());
    DCHECK_EQ(is_default_conflict, IsDefaultConflicting());
    DCHECK_EQ(is_compilable, IsCompilable());
    DCHECK_EQ(must_count_locks, MustCountLocks());
  } else {
    SetAccessFlags(new_value);
  }
}

template<ReadBarrierOption kReadBarrierOption, typename RootVisitorType>
void ArtMethod::VisitRoots(RootVisitorType& visitor, PointerSize pointer_size) {
  if (LIKELY(!declaring_class_.IsNull())) {
    visitor.VisitRoot(declaring_class_.AddressWithoutBarrier());
    mirror::Class* klass = declaring_class_.Read<kReadBarrierOption>();
    if (UNLIKELY(klass->IsProxyClass())) {
      // For normal methods, dex cache shortcuts will be visited through the declaring class.
      // However, for proxies we need to keep the interface method alive, so we visit its roots.
      ArtMethod* interface_method = mirror::DexCache::GetElementPtrSize(
          GetDexCacheResolvedMethods(pointer_size),
          GetDexMethodIndex(),
          pointer_size);
      DCHECK(interface_method != nullptr);
      DCHECK_EQ(interface_method,
                Runtime::Current()->GetClassLinker()->FindMethodForProxy<kReadBarrierOption>(
                    klass, this));
      interface_method->VisitRoots(visitor, pointer_size);
    }
  }
}

template <typename Visitor>
inline void ArtMethod::UpdateObjectsForImageRelocation(const Visitor& visitor,
                                                       PointerSize pointer_size) {
  mirror::Class* old_class = GetDeclaringClassUnchecked<kWithoutReadBarrier>();
  mirror::Class* new_class = visitor(old_class);
  if (old_class != new_class) {
    SetDeclaringClass(new_class);
  }
  ArtMethod** old_methods = GetDexCacheResolvedMethods(pointer_size);
  ArtMethod** new_methods = visitor(old_methods);
  if (old_methods != new_methods) {
    SetDexCacheResolvedMethods(new_methods, pointer_size);
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void ArtMethod::UpdateEntrypoints(const Visitor& visitor, PointerSize pointer_size) {
  if (IsNative<kReadBarrierOption>()) {
    const void* old_native_code = GetEntryPointFromJniPtrSize(pointer_size);
    const void* new_native_code = visitor(old_native_code);
    if (old_native_code != new_native_code) {
      SetEntryPointFromJniPtrSize(new_native_code, pointer_size);
    }
  } else {
    DCHECK(GetDataPtrSize(pointer_size) == nullptr);
  }
  const void* old_code = GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  const void* new_code = visitor(old_code);
  if (old_code != new_code) {
    SetEntryPointFromQuickCompiledCodePtrSize(new_code, pointer_size);
  }
}

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_INL_H_
