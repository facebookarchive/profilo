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

#ifndef ART_RUNTIME_ART_METHOD_H_
#define ART_RUNTIME_ART_METHOD_H_

#include "base/bit_utils.h"
#include "base/casts.h"
#include "dex_file.h"
#include "gc_root.h"
#include "invoke_type.h"
#include "method_reference.h"
#include "modifiers.h"
#include "mirror/object.h"
#include "read_barrier_option.h"
#include "stack.h"
#include "utils.h"

namespace art {

union JValue;
class OatQuickMethodHeader;
class ProfilingInfo;
class ScopedObjectAccessAlreadyRunnable;
class StringPiece;
class ShadowFrame;

namespace mirror {
class Array;
class Class;
class IfTable;
class PointerArray;
}  // namespace mirror

// Table to resolve IMT conflicts at runtime. The table is attached to
// the jni entrypoint of IMT conflict ArtMethods.
// The table contains a list of pairs of { interface_method, implementation_method }
// with the last entry being null to make an assembly implementation of a lookup
// faster.
class ImtConflictTable {
  enum MethodIndex {
    kMethodInterface,
    kMethodImplementation,
    kMethodCount,  // Number of elements in enum.
  };

 public:
  // Build a new table copying `other` and adding the new entry formed of
  // the pair { `interface_method`, `implementation_method` }
  ImtConflictTable(ImtConflictTable* other,
                   ArtMethod* interface_method,
                   ArtMethod* implementation_method,
                   size_t pointer_size) {
    const size_t count = other->NumEntries(pointer_size);
    for (size_t i = 0; i < count; ++i) {
      SetInterfaceMethod(i, pointer_size, other->GetInterfaceMethod(i, pointer_size));
      SetImplementationMethod(i, pointer_size, other->GetImplementationMethod(i, pointer_size));
    }
    SetInterfaceMethod(count, pointer_size, interface_method);
    SetImplementationMethod(count, pointer_size, implementation_method);
    // Add the null marker.
    SetInterfaceMethod(count + 1, pointer_size, nullptr);
    SetImplementationMethod(count + 1, pointer_size, nullptr);
  }

  // num_entries excludes the header.
  ImtConflictTable(size_t num_entries, size_t pointer_size) {
    SetInterfaceMethod(num_entries, pointer_size, nullptr);
    SetImplementationMethod(num_entries, pointer_size, nullptr);
  }

  // Set an entry at an index.
  void SetInterfaceMethod(size_t index, size_t pointer_size, ArtMethod* method) {
    SetMethod(index * kMethodCount + kMethodInterface, pointer_size, method);
  }

  void SetImplementationMethod(size_t index, size_t pointer_size, ArtMethod* method) {
    SetMethod(index * kMethodCount + kMethodImplementation, pointer_size, method);
  }

  ArtMethod* GetInterfaceMethod(size_t index, size_t pointer_size) const {
    return GetMethod(index * kMethodCount + kMethodInterface, pointer_size);
  }

  ArtMethod* GetImplementationMethod(size_t index, size_t pointer_size) const {
    return GetMethod(index * kMethodCount + kMethodImplementation, pointer_size);
  }

  // Return true if two conflict tables are the same.
  bool Equals(ImtConflictTable* other, size_t pointer_size) const {
    size_t num = NumEntries(pointer_size);
    if (num != other->NumEntries(pointer_size)) {
      return false;
    }
    for (size_t i = 0; i < num; ++i) {
      if (GetInterfaceMethod(i, pointer_size) != other->GetInterfaceMethod(i, pointer_size) ||
          GetImplementationMethod(i, pointer_size) !=
              other->GetImplementationMethod(i, pointer_size)) {
        return false;
      }
    }
    return true;
  }

  // Visit all of the entries.
  // NO_THREAD_SAFETY_ANALYSIS for calling with held locks. Visitor is passed a pair of ArtMethod*
  // and also returns one. The order is <interface, implementation>.
  template<typename Visitor>
  void Visit(const Visitor& visitor, size_t pointer_size) NO_THREAD_SAFETY_ANALYSIS {
    uint32_t table_index = 0;
    for (;;) {
      ArtMethod* interface_method = GetInterfaceMethod(table_index, pointer_size);
      if (interface_method == nullptr) {
        break;
      }
      ArtMethod* implementation_method = GetImplementationMethod(table_index, pointer_size);
      auto input = std::make_pair(interface_method, implementation_method);
      std::pair<ArtMethod*, ArtMethod*> updated = visitor(input);
      if (input.first != updated.first) {
        SetInterfaceMethod(table_index, pointer_size, updated.first);
      }
      if (input.second != updated.second) {
        SetImplementationMethod(table_index, pointer_size, updated.second);
      }
      ++table_index;
    }
  }

  // Lookup the implementation ArtMethod associated to `interface_method`. Return null
  // if not found.
  ArtMethod* Lookup(ArtMethod* interface_method, size_t pointer_size) const {
    uint32_t table_index = 0;
    for (;;) {
      ArtMethod* current_interface_method = GetInterfaceMethod(table_index, pointer_size);
      if (current_interface_method == nullptr) {
        break;
      }
      if (current_interface_method == interface_method) {
        return GetImplementationMethod(table_index, pointer_size);
      }
      ++table_index;
    }
    return nullptr;
  }

  // Compute the number of entries in this table.
  size_t NumEntries(size_t pointer_size) const {
    uint32_t table_index = 0;
    while (GetInterfaceMethod(table_index, pointer_size) != nullptr) {
      ++table_index;
    }
    return table_index;
  }

  // Compute the size in bytes taken by this table.
  size_t ComputeSize(size_t pointer_size) const {
    // Add the end marker.
    return ComputeSize(NumEntries(pointer_size), pointer_size);
  }

  // Compute the size in bytes needed for copying the given `table` and add
  // one more entry.
  static size_t ComputeSizeWithOneMoreEntry(ImtConflictTable* table, size_t pointer_size) {
    return table->ComputeSize(pointer_size) + EntrySize(pointer_size);
  }

  // Compute size with a fixed number of entries.
  static size_t ComputeSize(size_t num_entries, size_t pointer_size) {
    return (num_entries + 1) * EntrySize(pointer_size);  // Add one for null terminator.
  }

  static size_t EntrySize(size_t pointer_size) {
    return pointer_size * static_cast<size_t>(kMethodCount);
  }

 private:
  ArtMethod* GetMethod(size_t index, size_t pointer_size) const {
    if (pointer_size == 8) {
      return reinterpret_cast<ArtMethod*>(static_cast<uintptr_t>(data64_[index]));
    } else {
      DCHECK_EQ(pointer_size, 4u);
      return reinterpret_cast<ArtMethod*>(static_cast<uintptr_t>(data32_[index]));
    }
  }

  void SetMethod(size_t index, size_t pointer_size, ArtMethod* method) {
    if (pointer_size == 8) {
      data64_[index] = dchecked_integral_cast<uint64_t>(reinterpret_cast<uintptr_t>(method));
    } else {
      DCHECK_EQ(pointer_size, 4u);
      data32_[index] = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(method));
    }
  }

  // Array of entries that the assembly stubs will iterate over. Note that this is
  // not fixed size, and we allocate data prior to calling the constructor
  // of ImtConflictTable.
  union {
    uint32_t data32_[0];
    uint64_t data64_[0];
  };

  DISALLOW_COPY_AND_ASSIGN(ImtConflictTable);
};

class ArtMethod FINAL {
 public:
  ArtMethod() : access_flags_(0), dex_code_item_offset_(0), dex_method_index_(0),
      method_index_(0) { }

  ArtMethod(ArtMethod* src, size_t image_pointer_size) {
    CopyFrom(src, image_pointer_size);
  }

  static ArtMethod* FromReflectedMethod(const ScopedObjectAccessAlreadyRunnable& soa,
                                        jobject jlr_method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE mirror::Class* GetDeclaringClass() SHARED_REQUIRES(Locks::mutator_lock_);

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE mirror::Class* GetDeclaringClassUnchecked()
      SHARED_REQUIRES(Locks::mutator_lock_);

  void SetDeclaringClass(mirror::Class *new_declaring_class)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool CASDeclaringClass(mirror::Class* expected_class, mirror::Class* desired_class)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static MemberOffset DeclaringClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ArtMethod, declaring_class_));
  }

  // Note: GetAccessFlags acquires the mutator lock in debug mode to check that it is not called for
  // a proxy method.
  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE uint32_t GetAccessFlags();

  void SetAccessFlags(uint32_t new_access_flags) {
    // Not called within a transaction.
    access_flags_ = new_access_flags;
  }

  // Approximate what kind of method call would be used for this method.
  InvokeType GetInvokeType() SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns true if the method is declared public.
  bool IsPublic() {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  // Returns true if the method is declared private.
  bool IsPrivate() {
    return (GetAccessFlags() & kAccPrivate) != 0;
  }

  // Returns true if the method is declared static.
  bool IsStatic() {
    return (GetAccessFlags() & kAccStatic) != 0;
  }

  // Returns true if the method is a constructor.
  bool IsConstructor() {
    return (GetAccessFlags() & kAccConstructor) != 0;
  }

  // Returns true if the method is a class initializer.
  bool IsClassInitializer() {
    return IsConstructor() && IsStatic();
  }

  // Returns true if the method is static, private, or a constructor.
  bool IsDirect() {
    return IsDirect(GetAccessFlags());
  }

  static bool IsDirect(uint32_t access_flags) {
    constexpr uint32_t direct = kAccStatic | kAccPrivate | kAccConstructor;
    return (access_flags & direct) != 0;
  }

  // Returns true if the method is declared synchronized.
  bool IsSynchronized() {
    constexpr uint32_t synchonized = kAccSynchronized | kAccDeclaredSynchronized;
    return (GetAccessFlags() & synchonized) != 0;
  }

  bool IsFinal() {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  bool IsCopied() {
    const bool copied = (GetAccessFlags() & kAccCopied) != 0;
    // (IsMiranda() || IsDefaultConflicting()) implies copied
    DCHECK(!(IsMiranda() || IsDefaultConflicting()) || copied)
        << "Miranda or default-conflict methods must always be copied.";
    return copied;
  }

  bool IsMiranda() {
    return (GetAccessFlags() & kAccMiranda) != 0;
  }

  // Returns true if invoking this method will not throw an AbstractMethodError or
  // IncompatibleClassChangeError.
  bool IsInvokable() {
    return !IsAbstract() && !IsDefaultConflicting();
  }

  bool IsCompilable() {
    return (GetAccessFlags() & kAccCompileDontBother) == 0;
  }

  // A default conflict method is a special sentinel method that stands for a conflict between
  // multiple default methods. It cannot be invoked, throwing an IncompatibleClassChangeError if one
  // attempts to do so.
  bool IsDefaultConflicting() {
    return (GetAccessFlags() & kAccDefaultConflict) != 0u;
  }

  // This is set by the class linker.
  bool IsDefault() {
    return (GetAccessFlags() & kAccDefault) != 0;
  }

  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsNative() {
    return (GetAccessFlags<kReadBarrierOption>() & kAccNative) != 0;
  }

  bool IsFastNative() {
    constexpr uint32_t mask = kAccFastNative | kAccNative;
    return (GetAccessFlags() & mask) == mask;
  }

  bool IsAbstract() {
    return (GetAccessFlags() & kAccAbstract) != 0;
  }

  bool IsSynthetic() {
    return (GetAccessFlags() & kAccSynthetic) != 0;
  }

  bool IsProxyMethod() SHARED_REQUIRES(Locks::mutator_lock_);

  bool SkipAccessChecks() {
    return (GetAccessFlags() & kAccSkipAccessChecks) != 0;
  }

  void SetSkipAccessChecks() {
    DCHECK(!SkipAccessChecks());
    SetAccessFlags(GetAccessFlags() | kAccSkipAccessChecks);
  }

  // Should this method be run in the interpreter and count locks (e.g., failed structured-
  // locking verification)?
  bool MustCountLocks() {
    return (GetAccessFlags() & kAccMustCountLocks) != 0;
  }

  // Returns true if this method could be overridden by a default method.
  bool IsOverridableByDefaultMethod() SHARED_REQUIRES(Locks::mutator_lock_);

  bool CheckIncompatibleClassChange(InvokeType type) SHARED_REQUIRES(Locks::mutator_lock_);

  // Throws the error that would result from trying to invoke this method (i.e.
  // IncompatibleClassChangeError or AbstractMethodError). Only call if !IsInvokable();
  void ThrowInvocationTimeError() SHARED_REQUIRES(Locks::mutator_lock_);

  uint16_t GetMethodIndex() SHARED_REQUIRES(Locks::mutator_lock_);

  // Doesn't do erroneous / unresolved class checks.
  uint16_t GetMethodIndexDuringLinking() SHARED_REQUIRES(Locks::mutator_lock_);

  size_t GetVtableIndex() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetMethodIndex();
  }

  void SetMethodIndex(uint16_t new_method_index) SHARED_REQUIRES(Locks::mutator_lock_) {
    // Not called within a transaction.
    method_index_ = new_method_index;
  }

  static MemberOffset DexMethodIndexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, dex_method_index_);
  }

  static MemberOffset MethodIndexOffset() {
    return OFFSET_OF_OBJECT_MEMBER(ArtMethod, method_index_);
  }

  uint32_t GetCodeItemOffset() {
    return dex_code_item_offset_;
  }

  void SetCodeItemOffset(uint32_t new_code_off) {
    // Not called within a transaction.
    dex_code_item_offset_ = new_code_off;
  }

  // Number of 32bit registers that would be required to hold all the arguments
  static size_t NumArgRegisters(const StringPiece& shorty);

  ALWAYS_INLINE uint32_t GetDexMethodIndex() SHARED_REQUIRES(Locks::mutator_lock_);

  void SetDexMethodIndex(uint32_t new_idx) {
    // Not called within a transaction.
    dex_method_index_ = new_idx;
  }

  ALWAYS_INLINE ArtMethod** GetDexCacheResolvedMethods(size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  ALWAYS_INLINE ArtMethod* GetDexCacheResolvedMethod(uint16_t method_index, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  ALWAYS_INLINE void SetDexCacheResolvedMethod(uint16_t method_index,
                                               ArtMethod* new_method,
                                               size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  ALWAYS_INLINE void SetDexCacheResolvedMethods(ArtMethod** new_dex_cache_methods, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool HasDexCacheResolvedMethods(size_t pointer_size) SHARED_REQUIRES(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedMethods(ArtMethod* other, size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedMethods(ArtMethod** other_cache, size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  template <bool kWithCheck = true>
  mirror::Class* GetDexCacheResolvedType(uint32_t type_idx, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void SetDexCacheResolvedTypes(GcRoot<mirror::Class>* new_dex_cache_types, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool HasDexCacheResolvedTypes(size_t pointer_size) SHARED_REQUIRES(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedTypes(ArtMethod* other, size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool HasSameDexCacheResolvedTypes(GcRoot<mirror::Class>* other_cache, size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Get the Class* from the type index into this method's dex cache.
  mirror::Class* GetClassFromTypeIndex(uint16_t type_idx, bool resolve, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns true if this method has the same name and signature of the other method.
  bool HasSameNameAndSignature(ArtMethod* other) SHARED_REQUIRES(Locks::mutator_lock_);

  // Find the method that this method overrides.
  ArtMethod* FindOverriddenMethod(size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Find the method index for this method within other_dexfile. If this method isn't present then
  // return DexFile::kDexNoIndex. The name_and_signature_idx MUST refer to a MethodId with the same
  // name and signature in the other_dexfile, such as the method index used to resolve this method
  // in the other_dexfile.
  uint32_t FindDexMethodIndexInOtherDexFile(const DexFile& other_dexfile,
                                            uint32_t name_and_signature_idx)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void Invoke(Thread* self, uint32_t* args, uint32_t args_size, JValue* result, const char* shorty)
      SHARED_REQUIRES(Locks::mutator_lock_);

  const void* GetEntryPointFromQuickCompiledCode() {
    return GetEntryPointFromQuickCompiledCodePtrSize(sizeof(void*));
  }
  ALWAYS_INLINE const void* GetEntryPointFromQuickCompiledCodePtrSize(size_t pointer_size) {
    return GetNativePointer<const void*>(
        EntryPointFromQuickCompiledCodeOffset(pointer_size), pointer_size);
  }

  void SetEntryPointFromQuickCompiledCode(const void* entry_point_from_quick_compiled_code) {
    SetEntryPointFromQuickCompiledCodePtrSize(entry_point_from_quick_compiled_code,
                                              sizeof(void*));
  }
  ALWAYS_INLINE void SetEntryPointFromQuickCompiledCodePtrSize(
      const void* entry_point_from_quick_compiled_code, size_t pointer_size) {
    SetNativePointer(EntryPointFromQuickCompiledCodeOffset(pointer_size),
                     entry_point_from_quick_compiled_code, pointer_size);
  }

  void RegisterNative(const void* native_method, bool is_fast)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void UnregisterNative() SHARED_REQUIRES(Locks::mutator_lock_);

  static MemberOffset DexCacheResolvedMethodsOffset(size_t pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, dex_cache_resolved_methods_) / sizeof(void*) * pointer_size);
  }

  static MemberOffset DexCacheResolvedTypesOffset(size_t pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, dex_cache_resolved_types_) / sizeof(void*) * pointer_size);
  }

  static MemberOffset EntryPointFromJniOffset(size_t pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, entry_point_from_jni_) / sizeof(void*) * pointer_size);
  }

  static MemberOffset EntryPointFromQuickCompiledCodeOffset(size_t pointer_size) {
    return MemberOffset(PtrSizedFieldsOffset(pointer_size) + OFFSETOF_MEMBER(
        PtrSizedFields, entry_point_from_quick_compiled_code_) / sizeof(void*) * pointer_size);
  }

  ProfilingInfo* GetProfilingInfo(size_t pointer_size) {
    return reinterpret_cast<ProfilingInfo*>(GetEntryPointFromJniPtrSize(pointer_size));
  }

  ImtConflictTable* GetImtConflictTable(size_t pointer_size) {
    DCHECK(IsRuntimeMethod());
    return reinterpret_cast<ImtConflictTable*>(GetEntryPointFromJniPtrSize(pointer_size));
  }

  ALWAYS_INLINE void SetImtConflictTable(ImtConflictTable* table, size_t pointer_size) {
    SetEntryPointFromJniPtrSize(table, pointer_size);
  }

  ALWAYS_INLINE void SetProfilingInfo(ProfilingInfo* info) {
    SetEntryPointFromJniPtrSize(info, sizeof(void*));
  }

  ALWAYS_INLINE void SetProfilingInfoPtrSize(ProfilingInfo* info, size_t pointer_size) {
    SetEntryPointFromJniPtrSize(info, pointer_size);
  }

  static MemberOffset ProfilingInfoOffset() {
    return EntryPointFromJniOffset(sizeof(void*));
  }

  void* GetEntryPointFromJni() {
    return GetEntryPointFromJniPtrSize(sizeof(void*));
  }

  ALWAYS_INLINE void* GetEntryPointFromJniPtrSize(size_t pointer_size) {
    return GetNativePointer<void*>(EntryPointFromJniOffset(pointer_size), pointer_size);
  }

  void SetEntryPointFromJni(const void* entrypoint) {
    DCHECK(IsNative());
    SetEntryPointFromJniPtrSize(entrypoint, sizeof(void*));
  }

  ALWAYS_INLINE void SetEntryPointFromJniPtrSize(const void* entrypoint, size_t pointer_size) {
    SetNativePointer(EntryPointFromJniOffset(pointer_size), entrypoint, pointer_size);
  }

  // Is this a CalleSaveMethod or ResolutionMethod and therefore doesn't adhere to normal
  // conventions for a method of managed code. Returns false for Proxy methods.
  ALWAYS_INLINE bool IsRuntimeMethod();

  // Is this a hand crafted method used for something like describing callee saves?
  bool IsCalleeSaveMethod() SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsResolutionMethod() SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsImtUnimplementedMethod() SHARED_REQUIRES(Locks::mutator_lock_);

  MethodReference ToMethodReference() SHARED_REQUIRES(Locks::mutator_lock_) {
    return MethodReference(GetDexFile(), GetDexMethodIndex());
  }

  // Find the catch block for the given exception type and dex_pc. When a catch block is found,
  // indicates whether the found catch block is responsible for clearing the exception or whether
  // a move-exception instruction is present.
  uint32_t FindCatchBlock(Handle<mirror::Class> exception_type, uint32_t dex_pc,
                          bool* has_no_move_exception)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // NO_THREAD_SAFETY_ANALYSIS since we don't know what the callback requires.
  template<typename RootVisitorType>
  void VisitRoots(RootVisitorType& visitor, size_t pointer_size) NO_THREAD_SAFETY_ANALYSIS;

  const DexFile* GetDexFile() SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetDeclaringClassDescriptor() SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetShorty() SHARED_REQUIRES(Locks::mutator_lock_) {
    uint32_t unused_length;
    return GetShorty(&unused_length);
  }

  const char* GetShorty(uint32_t* out_length) SHARED_REQUIRES(Locks::mutator_lock_);

  const Signature GetSignature() SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE const char* GetName() SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::String* GetNameAsString(Thread* self) SHARED_REQUIRES(Locks::mutator_lock_);

  const DexFile::CodeItem* GetCodeItem() SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsResolvedTypeIdx(uint16_t type_idx, size_t ptr_size) SHARED_REQUIRES(Locks::mutator_lock_);

  int32_t GetLineNumFromDexPC(uint32_t dex_pc) SHARED_REQUIRES(Locks::mutator_lock_);

  const DexFile::ProtoId& GetPrototype() SHARED_REQUIRES(Locks::mutator_lock_);

  const DexFile::TypeList* GetParameterTypeList() SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetDeclaringClassSourceFile() SHARED_REQUIRES(Locks::mutator_lock_);

  uint16_t GetClassDefIndex() SHARED_REQUIRES(Locks::mutator_lock_);

  const DexFile::ClassDef& GetClassDef() SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetReturnTypeDescriptor() SHARED_REQUIRES(Locks::mutator_lock_);

  const char* GetTypeDescriptorFromTypeIdx(uint16_t type_idx)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // May cause thread suspension due to GetClassFromTypeIdx calling ResolveType this caused a large
  // number of bugs at call sites.
  mirror::Class* GetReturnType(bool resolve, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::ClassLoader* GetClassLoader() SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::DexCache* GetDexCache() SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetInterfaceMethodIfProxy(size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // May cause thread suspension due to class resolution.
  bool EqualParameters(Handle<mirror::ObjectArray<mirror::Class>> params)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Size of an instance of this native class.
  static size_t Size(size_t pointer_size) {
    return RoundUp(OFFSETOF_MEMBER(ArtMethod, ptr_sized_fields_), pointer_size) +
        (sizeof(PtrSizedFields) / sizeof(void*)) * pointer_size;
  }

  // Alignment of an instance of this native class.
  static size_t Alignment(size_t pointer_size) {
    // The ArtMethod alignment is the same as image pointer size. This differs from
    // alignof(ArtMethod) if cross-compiling with pointer_size != sizeof(void*).
    return pointer_size;
  }

  void CopyFrom(ArtMethod* src, size_t image_pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE GcRoot<mirror::Class>* GetDexCacheResolvedTypes(size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Note, hotness_counter_ updates are non-atomic but it doesn't need to be precise.  Also,
  // given that the counter is only 16 bits wide we can expect wrap-around in some
  // situations.  Consumers of hotness_count_ must be able to deal with that.
  uint16_t IncrementCounter() {
    return ++hotness_count_;
  }

  void ClearCounter() {
    hotness_count_ = 0;
  }

  void SetCounter(int16_t hotness_count) {
    hotness_count_ = hotness_count;
  }

  uint16_t GetCounter() const {
    return hotness_count_;
  }

  const uint8_t* GetQuickenedInfo() SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns the method header for the compiled code containing 'pc'. Note that runtime
  // methods will return null for this method, as they are not oat based.
  const OatQuickMethodHeader* GetOatQuickMethodHeader(uintptr_t pc)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns whether the method has any compiled code, JIT or AOT.
  bool HasAnyCompiledCode() SHARED_REQUIRES(Locks::mutator_lock_);


  // Update heap objects and non-entrypoint pointers by the passed in visitor for image relocation.
  // Does not use read barrier.
  template <typename Visitor>
  ALWAYS_INLINE void UpdateObjectsForImageRelocation(const Visitor& visitor, size_t pointer_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Update entry points by passing them through the visitor.
  template <ReadBarrierOption kReadBarrierOption = kWithReadBarrier, typename Visitor>
  ALWAYS_INLINE void UpdateEntrypoints(const Visitor& visitor, size_t pointer_size);

 protected:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  // The class we are a part of.
  GcRoot<mirror::Class> declaring_class_;

  // Access flags; low 16 bits are defined by spec.
  uint32_t access_flags_;

  /* Dex file fields. The defining dex file is available via declaring_class_->dex_cache_ */

  // Offset to the CodeItem.
  uint32_t dex_code_item_offset_;

  // Index into method_ids of the dex file associated with this method.
  uint32_t dex_method_index_;

  /* End of dex file fields. */

  // Entry within a dispatch table for this method. For static/direct methods the index is into
  // the declaringClass.directMethods, for virtual methods the vtable and for interface methods the
  // ifTable.
  uint16_t method_index_;

  // The hotness we measure for this method. Managed by the interpreter. Not atomic, as we allow
  // missing increments: if the method is hot, we will see it eventually.
  uint16_t hotness_count_;

  // Fake padding field gets inserted here.

  // Must be the last fields in the method.
  // PACKED(4) is necessary for the correctness of
  // RoundUp(OFFSETOF_MEMBER(ArtMethod, ptr_sized_fields_), pointer_size).
  struct PACKED(4) PtrSizedFields {
    // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
    ArtMethod** dex_cache_resolved_methods_;

    // Short cuts to declaring_class_->dex_cache_ member for fast compiled code access.
    GcRoot<mirror::Class>* dex_cache_resolved_types_;

    // Pointer to JNI function registered to this method, or a function to resolve the JNI function,
    // or the profiling data for non-native methods, or an ImtConflictTable.
    void* entry_point_from_jni_;

    // Method dispatch from quick compiled code invokes this pointer which may cause bridging into
    // the interpreter.
    void* entry_point_from_quick_compiled_code_;
  } ptr_sized_fields_;

 private:
  static size_t PtrSizedFieldsOffset(size_t pointer_size) {
    // Round up to pointer size for padding field.
    return RoundUp(OFFSETOF_MEMBER(ArtMethod, ptr_sized_fields_), pointer_size);
  }

  template<typename T>
  ALWAYS_INLINE T GetNativePointer(MemberOffset offset, size_t pointer_size) const {
    static_assert(std::is_pointer<T>::value, "T must be a pointer type");
    DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
    const auto addr = reinterpret_cast<uintptr_t>(this) + offset.Uint32Value();
    if (pointer_size == sizeof(uint32_t)) {
      return reinterpret_cast<T>(*reinterpret_cast<const uint32_t*>(addr));
    } else {
      auto v = *reinterpret_cast<const uint64_t*>(addr);
      return reinterpret_cast<T>(dchecked_integral_cast<uintptr_t>(v));
    }
  }

  template<typename T>
  ALWAYS_INLINE void SetNativePointer(MemberOffset offset, T new_value, size_t pointer_size) {
    static_assert(std::is_pointer<T>::value, "T must be a pointer type");
    DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
    const auto addr = reinterpret_cast<uintptr_t>(this) + offset.Uint32Value();
    if (pointer_size == sizeof(uint32_t)) {
      uintptr_t ptr = reinterpret_cast<uintptr_t>(new_value);
      *reinterpret_cast<uint32_t*>(addr) = dchecked_integral_cast<uint32_t>(ptr);
    } else {
      *reinterpret_cast<uint64_t*>(addr) = reinterpret_cast<uintptr_t>(new_value);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ArtMethod);  // Need to use CopyFrom to deal with 32 vs 64 bits.
};

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_H_
