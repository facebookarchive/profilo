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
#include "dex_file.h"
#include "dex_file-inl.h"
#include "gc_root-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object-inl.h"
#include "mirror/object_array.h"
#include "oat.h"
#include "quick/quick_method_frame_info.h"
#include "read_barrier-inl.h"
#include "runtime-inl.h"
#include "utils.h"

namespace art {

inline mirror::Class* ArtMethod::GetDeclaringClassUnchecked() {
  return declaring_class_.Read();
}

inline mirror::Class* ArtMethod::GetDeclaringClassNoBarrier() {
  return declaring_class_.Read<kWithoutReadBarrier>();
}

inline mirror::Class* ArtMethod::GetDeclaringClass() {
  mirror::Class* result = GetDeclaringClassUnchecked();
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

inline uint32_t ArtMethod::GetAccessFlags() {
  DCHECK(IsRuntimeMethod() || GetDeclaringClass()->IsIdxLoaded() ||
         GetDeclaringClass()->IsErroneous());
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

inline mirror::PointerArray* ArtMethod::GetDexCacheResolvedMethods() {
  return dex_cache_resolved_methods_.Read();
}

inline ArtMethod* ArtMethod::GetDexCacheResolvedMethod(uint16_t method_index, size_t ptr_size) {
  auto* method = GetDexCacheResolvedMethods()->GetElementPtrSize<ArtMethod*>(
      method_index, ptr_size);
  if (LIKELY(method != nullptr)) {
    auto* declaring_class = method->GetDeclaringClass();
    if (LIKELY(declaring_class == nullptr || !declaring_class->IsErroneous())) {
      return method;
    }
  }
  return nullptr;
}

inline void ArtMethod::SetDexCacheResolvedMethod(uint16_t method_idx, ArtMethod* new_method,
                                                 size_t ptr_size) {
  DCHECK(new_method == nullptr || new_method->GetDeclaringClass() != nullptr);
  GetDexCacheResolvedMethods()->SetElementPtrSize(method_idx, new_method, ptr_size);
}

inline bool ArtMethod::HasDexCacheResolvedMethods() {
  return GetDexCacheResolvedMethods() != nullptr;
}

inline bool ArtMethod::HasSameDexCacheResolvedMethods(mirror::PointerArray* other_cache) {
  return GetDexCacheResolvedMethods() == other_cache;
}

inline bool ArtMethod::HasSameDexCacheResolvedMethods(ArtMethod* other) {
  return GetDexCacheResolvedMethods() == other->GetDexCacheResolvedMethods();
}

inline mirror::ObjectArray<mirror::Class>* ArtMethod::GetDexCacheResolvedTypes() {
  return dex_cache_resolved_types_.Read();
}

template <bool kWithCheck>
inline mirror::Class* ArtMethod::GetDexCacheResolvedType(uint32_t type_index) {
  mirror::Class* klass = kWithCheck ?
      GetDexCacheResolvedTypes()->Get(type_index) :
      GetDexCacheResolvedTypes()->GetWithoutChecks(type_index);
  return (klass != nullptr && !klass->IsErroneous()) ? klass : nullptr;
}

inline bool ArtMethod::HasDexCacheResolvedTypes() {
  return GetDexCacheResolvedTypes() != nullptr;
}

inline bool ArtMethod::HasSameDexCacheResolvedTypes(
    mirror::ObjectArray<mirror::Class>* other_cache) {
  return GetDexCacheResolvedTypes() == other_cache;
}

inline bool ArtMethod::HasSameDexCacheResolvedTypes(ArtMethod* other) {
  return GetDexCacheResolvedTypes() == other->GetDexCacheResolvedTypes();
}

inline mirror::Class* ArtMethod::GetClassFromTypeIndex(uint16_t type_idx, bool resolve) {
  mirror::Class* type = GetDexCacheResolvedType(type_idx);
  if (type == nullptr && resolve) {
    type = Runtime::Current()->GetClassLinker()->ResolveType(type_idx, this);
    CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
  }
  return type;
}

inline uint32_t ArtMethod::GetCodeSize() {
  DCHECK(!IsRuntimeMethod() && !IsProxyMethod()) << PrettyMethod(this);
  return GetCodeSize(EntryPointToCodePointer(GetEntryPointFromQuickCompiledCode()));
}

inline uint32_t ArtMethod::GetCodeSize(const void* code) {
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].code_size_;
}

inline bool ArtMethod::CheckIncompatibleClassChange(InvokeType type) {
  switch (type) {
    case kStatic:
      return !IsStatic();
    case kDirect:
      return !IsDirect() || IsStatic();
    case kVirtual: {
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || (methods_class->IsInterface() && !IsMiranda());
    }
    case kSuper:
      // Constructors and static methods are called with invoke-direct.
      // Interface methods cannot be invoked with invoke-super.
      return IsConstructor() || IsStatic() || GetDeclaringClass()->IsInterface();
    case kInterface: {
      mirror::Class* methods_class = GetDeclaringClass();
      return IsDirect() || !(methods_class->IsInterface() || methods_class->IsObjectClass());
    }
    default:
      LOG(FATAL) << "Unreachable - invocation type: " << type;
      UNREACHABLE();
  }
}

inline uint32_t ArtMethod::GetQuickOatCodeOffset() {
  DCHECK(!Runtime::Current()->IsStarted());
  return PointerToLowMemUInt32(GetEntryPointFromQuickCompiledCode());
}

inline void ArtMethod::SetQuickOatCodeOffset(uint32_t code_offset) {
  DCHECK(!Runtime::Current()->IsStarted());
  SetEntryPointFromQuickCompiledCode(reinterpret_cast<void*>(code_offset));
}

inline const uint8_t* ArtMethod::GetMappingTable(size_t pointer_size) {
  const void* code_pointer = GetQuickOatCodePointer(pointer_size);
  if (code_pointer == nullptr) {
    return nullptr;
  }
  return GetMappingTable(code_pointer, pointer_size);
}

inline const uint8_t* ArtMethod::GetMappingTable(const void* code_pointer, size_t pointer_size) {
  DCHECK(code_pointer != nullptr);
  DCHECK_EQ(code_pointer, GetQuickOatCodePointer(pointer_size));
  uint32_t offset =
      reinterpret_cast<const OatQuickMethodHeader*>(code_pointer)[-1].mapping_table_offset_;
  if (UNLIKELY(offset == 0u)) {
    return nullptr;
  }
  return reinterpret_cast<const uint8_t*>(code_pointer) - offset;
}

inline const uint8_t* ArtMethod::GetVmapTable(size_t pointer_size) {
  const void* code_pointer = GetQuickOatCodePointer(pointer_size);
  if (code_pointer == nullptr) {
    return nullptr;
  }
  return GetVmapTable(code_pointer, pointer_size);
}

inline const uint8_t* ArtMethod::GetVmapTable(const void* code_pointer, size_t pointer_size) {
  CHECK(!IsOptimized(pointer_size)) << "Unimplemented vmap table for optimized compiler";
  DCHECK(code_pointer != nullptr);
  DCHECK_EQ(code_pointer, GetQuickOatCodePointer(pointer_size));
  uint32_t offset =
      reinterpret_cast<const OatQuickMethodHeader*>(code_pointer)[-1].vmap_table_offset_;
  if (UNLIKELY(offset == 0u)) {
    return nullptr;
  }
  return reinterpret_cast<const uint8_t*>(code_pointer) - offset;
}

inline CodeInfo ArtMethod::GetOptimizedCodeInfo() {
  DCHECK(IsOptimized(sizeof(void*)));
  const void* code_pointer = GetQuickOatCodePointer(sizeof(void*));
  DCHECK(code_pointer != nullptr);
  uint32_t offset =
      reinterpret_cast<const OatQuickMethodHeader*>(code_pointer)[-1].vmap_table_offset_;
  const void* data =
      reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(code_pointer) - offset);
  return CodeInfo(data);
}

inline const uint8_t* ArtMethod::GetNativeGcMap(size_t pointer_size) {
  const void* code_pointer = GetQuickOatCodePointer(pointer_size);
  if (code_pointer == nullptr) {
    return nullptr;
  }
  return GetNativeGcMap(code_pointer, pointer_size);
}

inline const uint8_t* ArtMethod::GetNativeGcMap(const void* code_pointer, size_t pointer_size) {
  DCHECK(code_pointer != nullptr);
  DCHECK_EQ(code_pointer, GetQuickOatCodePointer(pointer_size));
  uint32_t offset =
      reinterpret_cast<const OatQuickMethodHeader*>(code_pointer)[-1].gc_map_offset_;
  if (UNLIKELY(offset == 0u)) {
    return nullptr;
  }
  return reinterpret_cast<const uint8_t*>(code_pointer) - offset;
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

inline bool ArtMethod::IsImtConflictMethod() {
  bool result = this == Runtime::Current()->GetImtConflictMethod();
  // Check that if we do think it is phony it looks like the imt conflict method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline bool ArtMethod::IsImtUnimplementedMethod() {
  bool result = this == Runtime::Current()->GetImtUnimplementedMethod();
  // Check that if we do think it is phony it looks like the imt unimplemented method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline uintptr_t ArtMethod::NativeQuickPcOffset(const uintptr_t pc) {
  const void* code = Runtime::Current()->GetInstrumentation()->GetQuickCodeFor(
      this, sizeof(void*));
  return pc - reinterpret_cast<uintptr_t>(code);
}

inline QuickMethodFrameInfo ArtMethod::GetQuickFrameInfo(const void* code_pointer) {
  DCHECK(code_pointer != nullptr);
  if (kIsDebugBuild && !IsProxyMethod()) {
    CHECK_EQ(code_pointer, GetQuickOatCodePointer(sizeof(void*)));
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code_pointer)[-1].frame_info_;
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

inline bool ArtMethod::IsResolvedTypeIdx(uint16_t type_idx) {
  DCHECK(!IsProxyMethod());
  return GetDexCacheResolvedType(type_idx) != nullptr;
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
  auto interface_method = GetDexCacheResolvedMethods()->GetElementPtrSize<ArtMethod*>(
      GetDexMethodIndex(), pointer_size);
  DCHECK(interface_method != nullptr);
  DCHECK_EQ(interface_method,
            Runtime::Current()->GetClassLinker()->FindMethodForProxy(klass, this));
  return interface_method;
}

inline void ArtMethod::SetDexCacheResolvedMethods(mirror::PointerArray* new_dex_cache_methods) {
  dex_cache_resolved_methods_ = GcRoot<mirror::PointerArray>(new_dex_cache_methods);
}

inline void ArtMethod::SetDexCacheResolvedTypes(
    mirror::ObjectArray<mirror::Class>* new_dex_cache_types) {
  dex_cache_resolved_types_ = GcRoot<mirror::ObjectArray<mirror::Class>>(new_dex_cache_types);
}

inline mirror::Class* ArtMethod::GetReturnType(bool resolve) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  uint16_t return_type_idx = proto_id.return_type_idx_;
  mirror::Class* type = GetDexCacheResolvedType(return_type_idx);
  if (type == nullptr && resolve) {
    type = Runtime::Current()->GetClassLinker()->ResolveType(return_type_idx, this);
    CHECK(type != nullptr || Thread::Current()->IsExceptionPending());
  }
  return type;
}

template<typename RootVisitorType>
void ArtMethod::VisitRoots(RootVisitorType& visitor) {
  visitor.VisitRootIfNonNull(declaring_class_.AddressWithoutBarrier());
  visitor.VisitRootIfNonNull(dex_cache_resolved_methods_.AddressWithoutBarrier());
  visitor.VisitRootIfNonNull(dex_cache_resolved_types_.AddressWithoutBarrier());
}

inline void ArtMethod::CopyFrom(const ArtMethod* src, size_t image_pointer_size) {
  memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(src),
         ObjectSize(image_pointer_size));
  declaring_class_ = GcRoot<mirror::Class>(const_cast<ArtMethod*>(src)->GetDeclaringClass());
  dex_cache_resolved_methods_ = GcRoot<mirror::PointerArray>(
      const_cast<ArtMethod*>(src)->GetDexCacheResolvedMethods());
  dex_cache_resolved_types_ = GcRoot<mirror::ObjectArray<mirror::Class>>(
      const_cast<ArtMethod*>(src)->GetDexCacheResolvedTypes());
}

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_INL_H_
