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
#include "dex_file-inl.h"
#include "gc_root-inl.h"
#include "jit/profiling_info.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array.h"
#include "oat.h"
#include "quick/quick_method_frame_info.h"
#include "read_barrier-inl.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change.h"
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
      CHECK(result->IsIdxLoaded() || result->IsErroneous())
          << result->GetStatus() << " " << PrettyClass(result);
    } else {
      CHECK(result == nullptr) << this;
    }
  }
  return result;
}

inline void ArtMethod::SetDeclaringClass(mirror::Class* new_declaring_class) {
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

// AssertSharedHeld doesn't work in GetAccessFlags, so use a NO_THREAD_SAFETY_ANALYSIS helper.
// TODO: Figure out why ASSERT_SHARED_CAPABILITY doesn't work.
template <ReadBarrierOption kReadBarrierOption>
ALWAYS_INLINE static inline void DoGetAccessFlagsHelper(ArtMethod* method)
    NO_THREAD_SAFETY_ANALYSIS {
  CHECK(method->IsRuntimeMethod() ||
        method->GetDeclaringClass<kReadBarrierOption>()->IsIdxLoaded() ||
        method->GetDeclaringClass<kReadBarrierOption>()->IsErroneous());
}

template <ReadBarrierOption kReadBarrierOption>
inline uint32_t ArtMethod::GetAccessFlags() {
  if (kIsDebugBuild) {
    Thread* self = Thread::Current();
    if (!Locks::mutator_lock_->IsSharedHeld(self)) {
      ScopedObjectAccess soa(self);
      CHECK(IsRuntimeMethod() ||
            GetDeclaringClass<kReadBarrierOption>()->IsIdxLoaded() ||
            GetDeclaringClass<kReadBarrierOption>()->IsErroneous());
    } else {
      // We cannot use SOA in this case. We might be holding the lock, but may not be in the
      // runnable state (e.g., during GC).
      Locks::mutator_lock_->AssertSharedHeld(self);
      DoGetAccessFlagsHelper<kReadBarrierOption>(this);
    }
  }
  return access_flags_;
}

inline uint16_t ArtMethod::GetMethodIndex() {
  DCHECK(IsRuntimeMethod() || GetDeclaringClass()->IsResolved() ||
         GetDeclaringClass()->IsErroneous());
  return method_index_;
}

inline uint16_t ArtMethod::GetMethodIndexDuringLinking() {
  return method_index_;
}

inline uint32_t ArtMethod::GetDexMethodIndex() {
  DCHECK(IsRuntimeMethod() || GetDeclaringClass()->IsIdxLoaded() ||
         GetDeclaringClass()->IsErroneous());
  return dex_method_index_;
}

inline ArtMethod** ArtMethod::GetDexCacheResolvedMethods(size_t pointer_size) {
  return GetNativePointer<ArtMethod**>(DexCacheResolvedMethodsOffset(pointer_size),
                                       pointer_size);
}

inline ArtMethod* ArtMethod::GetDexCacheResolvedMethod(uint16_t method_index, size_t ptr_size) {
  // NOTE: Unchecked, i.e. not throwing AIOOB. We don't even know the length here
  // without accessing the DexCache and we don't want to do that in release build.
  DCHECK_LT(method_index,
            GetInterfaceMethodIfProxy(ptr_size)->GetDeclaringClass()
                ->GetDexCache()->NumResolvedMethods());
  ArtMethod* method = mirror::DexCache::GetElementPtrSize(GetDexCacheResolvedMethods(ptr_size),
                                                          method_index,
                                                          ptr_size);
  if (LIKELY(method != nullptr)) {
    auto* declaring_class = method->GetDeclaringClass();
    if (LIKELY(declaring_class == nullptr || !declaring_class->IsErroneous())) {
      return method;
    }
  }
  return nullptr;
}

inline void ArtMethod::SetDexCacheResolvedMethod(uint16_t method_index, ArtMethod* new_method,
                                                 size_t ptr_size) {
  // NOTE: Unchecked, i.e. not throwing AIOOB. We don't even know the length here
  // without accessing the DexCache and we don't want to do that in release build.
  DCHECK_LT(method_index,
            GetInterfaceMethodIfProxy(ptr_size)->GetDeclaringClass()
                ->GetDexCache()->NumResolvedMethods());
  DCHECK(new_method == nullptr || new_method->GetDeclaringClass() != nullptr);
  mirror::DexCache::SetElementPtrSize(GetDexCacheResolvedMethods(ptr_size),
                                      method_index,
                                      new_method,
                                      ptr_size);
}

inline bool ArtMethod::HasDexCacheResolvedMethods(size_t pointer_size) {
  return GetDexCacheResolvedMethods(pointer_size) != nullptr;
}

inline bool ArtMethod::HasSameDexCacheResolvedMethods(ArtMethod** other_cache,
                                                      size_t pointer_size) {
  return GetDexCacheResolvedMethods(pointer_size) == other_cache;
}

inline bool ArtMethod::HasSameDexCacheResolvedMethods(ArtMethod* other, size_t pointer_size) {
  return GetDexCacheResolvedMethods(pointer_size) ==
      other->GetDexCacheResolvedMethods(pointer_size);
}

inline GcRoot<mirror::Class>* ArtMethod::GetDexCacheResolvedTypes(size_t pointer_size) {
  return GetNativePointer<GcRoot<mirror::Class>*>(DexCacheResolvedTypesOffset(pointer_size),
                                                  pointer_size);
}

template <bool kWithCheck>
inline mirror::Class* ArtMethod::GetDexCacheResolvedType(uint32_t type_index, size_t ptr_size) {
  if (kWithCheck) {
    mirror::DexCache* dex_cache =
        GetInterfaceMethodIfProxy(ptr_size)->GetDeclaringClass()->GetDexCache();
    if (UNLIKELY(type_index >= dex_cache->NumResolvedTypes())) {
      ThrowArrayIndexOutOfBoundsException(type_index, dex_cache->NumResolvedTypes());
      return nullptr;
    }
  }
  mirror::Class* klass = GetDexCacheResolvedTypes(ptr_size)[type_index].Read();
  return (klass != nullptr && !klass->IsErroneous()) ? klass : nullptr;
}

inline bool ArtMethod::HasDexCacheResolvedTypes(size_t pointer_size) {
  return GetDexCacheResolvedTypes(pointer_size) != nullptr;
}

inline bool ArtMethod::HasSameDexCacheResolvedTypes(GcRoot<mirror::Class>* other_cache,
                                                    size_t pointer_size) {
  return GetDexCacheResolvedTypes(pointer_size) == other_cache;
}

inline bool ArtMethod::HasSameDexCacheResolvedTypes(ArtMethod* other, size_t pointer_size) {
  return GetDexCacheResolvedTypes(pointer_size) == other->GetDexCacheResolvedTypes(pointer_size);
}

inline mirror::Class* ArtMethod::GetClassFromTypeIndex(uint16_t type_idx,
                                                       bool resolve,
                                                       size_t ptr_size) {
  mirror::Class* type = GetDexCacheResolvedType(type_idx, ptr_size);
  if (type == nullptr && resolve) {
    type = Runtime::Current()->GetClassLinker()->ResolveType(type_idx, this);
    CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
  }
  return type;
}

inline bool ArtMethod::CheckIncompatibleClassChange(InvokeType type) {
  switch (type) {
    case kStatic:
      return !IsStatic();
    case kDirect:
      return !IsDirect() || IsStatic();
    case kVirtual: {
      // We have an error if we are direct or a non-default, non-miranda interface method.
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || (methods_class->IsInterface() && !IsDefault() && !IsMiranda());
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

inline bool ArtMethod::IsRuntimeMethod() {
  return dex_method_index_ == DexFile::kDexNoIndex;
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
  return GetDexCache()->GetDexFile();
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
  } else if (this == runtime->GetCalleeSaveMethod(Runtime::kSaveAll)) {
    return "<runtime internal callee-save all registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(Runtime::kRefsOnly)) {
    return "<runtime internal callee-save reference registers method>";
  } else if (this == runtime->GetCalleeSaveMethod(Runtime::kRefsAndArgs)) {
    return "<runtime internal callee-save reference and argument registers method>";
  } else {
    return "<unknown runtime internal method>";
  }
}

inline const DexFile::CodeItem* ArtMethod::GetCodeItem() {
  return GetDeclaringClass()->GetDexFile().GetCodeItem(GetCodeItemOffset());
}

inline bool ArtMethod::IsResolvedTypeIdx(uint16_t type_idx, size_t ptr_size) {
  DCHECK(!IsProxyMethod());
  return GetDexCacheResolvedType(type_idx, ptr_size) != nullptr;
}

inline int32_t ArtMethod::GetLineNumFromDexPC(uint32_t dex_pc) {
  DCHECK(!IsProxyMethod());
  if (dex_pc == DexFile::kDexNoIndex) {
    return IsNative() ? -2 : -1;
  }
  return GetDexFile()->GetLineNumFromPC(this, dex_pc);
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
  return GetDeclaringClass()->GetDexClassDefIndex();
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
  uint16_t return_type_idx = proto_id.return_type_idx_;
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(return_type_idx));
}

inline const char* ArtMethod::GetTypeDescriptorFromTypeIdx(uint16_t type_idx) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(type_idx));
}

inline mirror::ClassLoader* ArtMethod::GetClassLoader() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetClassLoader();
}

inline mirror::DexCache* ArtMethod::GetDexCache() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetDexCache();
}

inline bool ArtMethod::IsProxyMethod() {
  return GetDeclaringClass()->IsProxyClass();
}

inline ArtMethod* ArtMethod::GetInterfaceMethodIfProxy(size_t pointer_size) {
  if (LIKELY(!IsProxyMethod())) {
    return this;
  }
  mirror::Class* klass = GetDeclaringClass();
  ArtMethod* interface_method = mirror::DexCache::GetElementPtrSize(
      GetDexCacheResolvedMethods(pointer_size),
      GetDexMethodIndex(),
      pointer_size);
  DCHECK(interface_method != nullptr);
  DCHECK_EQ(interface_method,
            Runtime::Current()->GetClassLinker()->FindMethodForProxy(klass, this));
  return interface_method;
}

inline void ArtMethod::SetDexCacheResolvedMethods(ArtMethod** new_dex_cache_methods,
                                                  size_t ptr_size) {
  SetNativePointer(DexCacheResolvedMethodsOffset(ptr_size), new_dex_cache_methods, ptr_size);
}

inline void ArtMethod::SetDexCacheResolvedTypes(GcRoot<mirror::Class>* new_dex_cache_types,
                                                size_t ptr_size) {
  SetNativePointer(DexCacheResolvedTypesOffset(ptr_size), new_dex_cache_types, ptr_size);
}

inline mirror::Class* ArtMethod::GetReturnType(bool resolve, size_t ptr_size) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  uint16_t return_type_idx = proto_id.return_type_idx_;
  mirror::Class* type = GetDexCacheResolvedType(return_type_idx, ptr_size);
  if (type == nullptr && resolve) {
    type = Runtime::Current()->GetClassLinker()->ResolveType(return_type_idx, this);
    CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
  }
  return type;
}

template<typename RootVisitorType>
void ArtMethod::VisitRoots(RootVisitorType& visitor, size_t pointer_size) {
  ArtMethod* interface_method = nullptr;
  mirror::Class* klass = declaring_class_.Read();
  if (LIKELY(klass != nullptr)) {
    if (UNLIKELY(klass->IsProxyClass())) {
      // For normal methods, dex cache shortcuts will be visited through the declaring class.
      // However, for proxies we need to keep the interface method alive, so we visit its roots.
      interface_method = mirror::DexCache::GetElementPtrSize(
          GetDexCacheResolvedMethods(pointer_size),
          GetDexMethodIndex(),
          pointer_size);
      DCHECK(interface_method != nullptr);
      DCHECK_EQ(interface_method,
                Runtime::Current()->GetClassLinker()->FindMethodForProxy(klass, this));
      interface_method->VisitRoots(visitor, pointer_size);
    }
    visitor.VisitRoot(declaring_class_.AddressWithoutBarrier());
    // We know we don't have profiling information if the class hasn't been verified. Note
    // that this check also ensures the IsNative call can be made, as IsNative expects a fully
    // created class (and not a retired one).
    if (klass->IsVerified()) {
      // Runtime methods and native methods use the same field as the profiling info for
      // storing their own data (jni entrypoint for native methods, and ImtConflictTable for
      // some runtime methods).
      if (!IsNative() && !IsRuntimeMethod()) {
        ProfilingInfo* profiling_info = GetProfilingInfo(pointer_size);
        if (profiling_info != nullptr) {
          profiling_info->VisitRoots(visitor);
        }
      }
    }
  }
}

template <typename Visitor>
inline void ArtMethod::UpdateObjectsForImageRelocation(const Visitor& visitor,
                                                       size_t pointer_size) {
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
  GcRoot<mirror::Class>* old_types = GetDexCacheResolvedTypes(pointer_size);
  GcRoot<mirror::Class>* new_types = visitor(old_types);
  if (old_types != new_types) {
    SetDexCacheResolvedTypes(new_types, pointer_size);
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void ArtMethod::UpdateEntrypoints(const Visitor& visitor, size_t pointer_size) {
  if (IsNative<kReadBarrierOption>()) {
    const void* old_native_code = GetEntryPointFromJniPtrSize(pointer_size);
    const void* new_native_code = visitor(old_native_code);
    if (old_native_code != new_native_code) {
      SetEntryPointFromJniPtrSize(new_native_code, pointer_size);
    }
  } else {
    DCHECK(GetEntryPointFromJniPtrSize(pointer_size) == nullptr);
  }
  const void* old_code = GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  const void* new_code = visitor(old_code);
  if (old_code != new_code) {
    SetEntryPointFromQuickCompiledCodePtrSize(new_code, pointer_size);
  }
}

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_INL_H_
