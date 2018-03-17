/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
#define ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_

#include "dex_cache.h"

#include "art_field.h"
#include "art_method.h"
#include "base/casts.h"
#include "base/enums.h"
#include "base/logging.h"
#include "class_linker.h"
#include "dex_file.h"
#include "gc_root.h"
#include "gc/heap-inl.h"
#include "mirror/class.h"
#include "mirror/call_site.h"
#include "mirror/method_type.h"
#include "runtime.h"
#include "obj_ptr.h"

#include <atomic>

namespace art {
namespace mirror {

template <typename T>
inline void NativeDexCachePair<T>::Initialize(std::atomic<NativeDexCachePair<T>>* dex_cache,
                                              PointerSize pointer_size) {
  NativeDexCachePair<T> first_elem;
  first_elem.object = nullptr;
  first_elem.index = InvalidIndexForSlot(0);
  DexCache::SetNativePairPtrSize(dex_cache, 0, first_elem, pointer_size);
}

inline uint32_t DexCache::ClassSize(PointerSize pointer_size) {
  const uint32_t vtable_entries = Object::kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

inline uint32_t DexCache::StringSlotIndex(dex::StringIndex string_idx) {
  DCHECK_LT(string_idx.index_, GetDexFile()->NumStringIds());
  const uint32_t slot_idx = string_idx.index_ % kDexCacheStringCacheSize;
  DCHECK_LT(slot_idx, NumStrings());
  return slot_idx;
}

inline String* DexCache::GetResolvedString(dex::StringIndex string_idx) {
  return GetStrings()[StringSlotIndex(string_idx)].load(
      std::memory_order_relaxed).GetObjectForIndex(string_idx.index_);
}

inline void DexCache::SetResolvedString(dex::StringIndex string_idx, ObjPtr<String> resolved) {
  DCHECK(resolved != nullptr);
  GetStrings()[StringSlotIndex(string_idx)].store(
      StringDexCachePair(resolved, string_idx.index_), std::memory_order_relaxed);
  Runtime* const runtime = Runtime::Current();
  if (UNLIKELY(runtime->IsActiveTransaction())) {
    DCHECK(runtime->IsAotCompiler());
    runtime->RecordResolveString(this, string_idx);
  }
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  runtime->GetHeap()->WriteBarrierEveryFieldOf(this);
}

inline void DexCache::ClearString(dex::StringIndex string_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  uint32_t slot_idx = StringSlotIndex(string_idx);
  StringDexCacheType* slot = &GetStrings()[slot_idx];
  // This is racy but should only be called from the transactional interpreter.
  if (slot->load(std::memory_order_relaxed).index == string_idx.index_) {
    StringDexCachePair cleared(nullptr, StringDexCachePair::InvalidIndexForSlot(slot_idx));
    slot->store(cleared, std::memory_order_relaxed);
  }
}

inline uint32_t DexCache::TypeSlotIndex(dex::TypeIndex type_idx) {
  DCHECK_LT(type_idx.index_, GetDexFile()->NumTypeIds());
  const uint32_t slot_idx = type_idx.index_ % kDexCacheTypeCacheSize;
  DCHECK_LT(slot_idx, NumResolvedTypes());
  return slot_idx;
}

inline Class* DexCache::GetResolvedType(dex::TypeIndex type_idx) {
  // It is theorized that a load acquire is not required since obtaining the resolved class will
  // always have an address dependency or a lock.
  return GetResolvedTypes()[TypeSlotIndex(type_idx)].load(
      std::memory_order_relaxed).GetObjectForIndex(type_idx.index_);
}

inline void DexCache::SetResolvedType(dex::TypeIndex type_idx, ObjPtr<Class> resolved) {
  DCHECK(resolved != nullptr);
  // TODO default transaction support.
  // Use a release store for SetResolvedType. This is done to prevent other threads from seeing a
  // class but not necessarily seeing the loaded members like the static fields array.
  // See b/32075261.
  GetResolvedTypes()[TypeSlotIndex(type_idx)].store(
      TypeDexCachePair(resolved, type_idx.index_), std::memory_order_release);
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  Runtime::Current()->GetHeap()->WriteBarrierEveryFieldOf(this);
}

inline void DexCache::ClearResolvedType(dex::TypeIndex type_idx) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  uint32_t slot_idx = TypeSlotIndex(type_idx);
  TypeDexCacheType* slot = &GetResolvedTypes()[slot_idx];
  // This is racy but should only be called from the single-threaded ImageWriter and tests.
  if (slot->load(std::memory_order_relaxed).index == type_idx.index_) {
    TypeDexCachePair cleared(nullptr, TypeDexCachePair::InvalidIndexForSlot(slot_idx));
    slot->store(cleared, std::memory_order_relaxed);
  }
}

inline uint32_t DexCache::MethodTypeSlotIndex(uint32_t proto_idx) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  DCHECK_LT(proto_idx, GetDexFile()->NumProtoIds());
  const uint32_t slot_idx = proto_idx % kDexCacheMethodTypeCacheSize;
  DCHECK_LT(slot_idx, NumResolvedMethodTypes());
  return slot_idx;
}

inline MethodType* DexCache::GetResolvedMethodType(uint32_t proto_idx) {
  return GetResolvedMethodTypes()[MethodTypeSlotIndex(proto_idx)].load(
      std::memory_order_relaxed).GetObjectForIndex(proto_idx);
}

inline void DexCache::SetResolvedMethodType(uint32_t proto_idx, MethodType* resolved) {
  DCHECK(resolved != nullptr);
  GetResolvedMethodTypes()[MethodTypeSlotIndex(proto_idx)].store(
      MethodTypeDexCachePair(resolved, proto_idx), std::memory_order_relaxed);
  // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
  Runtime::Current()->GetHeap()->WriteBarrierEveryFieldOf(this);
}

inline CallSite* DexCache::GetResolvedCallSite(uint32_t call_site_idx) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());
  GcRoot<mirror::CallSite>& target = GetResolvedCallSites()[call_site_idx];
  Atomic<GcRoot<mirror::CallSite>>& ref =
      reinterpret_cast<Atomic<GcRoot<mirror::CallSite>>&>(target);
  return ref.LoadSequentiallyConsistent().Read();
}

inline CallSite* DexCache::SetResolvedCallSite(uint32_t call_site_idx, CallSite* call_site) {
  DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
  DCHECK_LT(call_site_idx, GetDexFile()->NumCallSiteIds());

  GcRoot<mirror::CallSite> null_call_site(nullptr);
  GcRoot<mirror::CallSite> candidate(call_site);
  GcRoot<mirror::CallSite>& target = GetResolvedCallSites()[call_site_idx];

  // The first assignment for a given call site wins.
  Atomic<GcRoot<mirror::CallSite>>& ref =
      reinterpret_cast<Atomic<GcRoot<mirror::CallSite>>&>(target);
  if (ref.CompareExchangeStrongSequentiallyConsistent(null_call_site, candidate)) {
    // TODO: Fine-grained marking, so that we don't need to go through all arrays in full.
    Runtime::Current()->GetHeap()->WriteBarrierEveryFieldOf(this);
    return call_site;
  } else {
    return target.Read();
  }
}

inline uint32_t DexCache::FieldSlotIndex(uint32_t field_idx) {
  DCHECK_LT(field_idx, GetDexFile()->NumFieldIds());
  const uint32_t slot_idx = field_idx % kDexCacheFieldCacheSize;
  DCHECK_LT(slot_idx, NumResolvedFields());
  return slot_idx;
}

inline ArtField* DexCache::GetResolvedField(uint32_t field_idx, PointerSize ptr_size) {
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), ptr_size);
  auto pair = GetNativePairPtrSize(GetResolvedFields(), FieldSlotIndex(field_idx), ptr_size);
  return pair.GetObjectForIndex(field_idx);
}

inline void DexCache::SetResolvedField(uint32_t field_idx, ArtField* field, PointerSize ptr_size) {
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), ptr_size);
  DCHECK(field != nullptr);
  FieldDexCachePair pair(field, field_idx);
  SetNativePairPtrSize(GetResolvedFields(), FieldSlotIndex(field_idx), pair, ptr_size);
}

inline void DexCache::ClearResolvedField(uint32_t field_idx, PointerSize ptr_size) {
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), ptr_size);
  uint32_t slot_idx = FieldSlotIndex(field_idx);
  auto* resolved_fields = GetResolvedFields();
  // This is racy but should only be called from the single-threaded ImageWriter.
  DCHECK(Runtime::Current()->IsAotCompiler());
  if (GetNativePairPtrSize(resolved_fields, slot_idx, ptr_size).index == field_idx) {
    FieldDexCachePair cleared(nullptr, FieldDexCachePair::InvalidIndexForSlot(slot_idx));
    SetNativePairPtrSize(resolved_fields, slot_idx, cleared, ptr_size);
  }
}

inline ArtMethod* DexCache::GetResolvedMethod(uint32_t method_idx, PointerSize ptr_size) {
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), ptr_size);
  DCHECK_LT(method_idx, NumResolvedMethods());  // NOTE: Unchecked, i.e. not throwing AIOOB.
  ArtMethod* method = GetElementPtrSize<ArtMethod*>(GetResolvedMethods(), method_idx, ptr_size);
  // Hide resolution trampoline methods from the caller
  if (method != nullptr && method->IsRuntimeMethod()) {
    DCHECK_EQ(method, Runtime::Current()->GetResolutionMethod());
    return nullptr;
  }
  return method;
}

inline void DexCache::SetResolvedMethod(uint32_t method_idx,
                                        ArtMethod* method,
                                        PointerSize ptr_size) {
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), ptr_size);
  DCHECK_LT(method_idx, NumResolvedMethods());  // NOTE: Unchecked, i.e. not throwing AIOOB.
  SetElementPtrSize(GetResolvedMethods(), method_idx, method, ptr_size);
}

template <typename PtrType>
inline PtrType DexCache::GetElementPtrSize(PtrType* ptr_array, size_t idx, PointerSize ptr_size) {
  if (ptr_size == PointerSize::k64) {
    uint64_t element = reinterpret_cast<const uint64_t*>(ptr_array)[idx];
    return reinterpret_cast<PtrType>(dchecked_integral_cast<uintptr_t>(element));
  } else {
    uint32_t element = reinterpret_cast<const uint32_t*>(ptr_array)[idx];
    return reinterpret_cast<PtrType>(dchecked_integral_cast<uintptr_t>(element));
  }
}

template <typename PtrType>
inline void DexCache::SetElementPtrSize(PtrType* ptr_array,
                                        size_t idx,
                                        PtrType ptr,
                                        PointerSize ptr_size) {
  if (ptr_size == PointerSize::k64) {
    reinterpret_cast<uint64_t*>(ptr_array)[idx] =
        dchecked_integral_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
  } else {
    reinterpret_cast<uint32_t*>(ptr_array)[idx] =
        dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(ptr));
  }
}

template <typename T>
NativeDexCachePair<T> DexCache::GetNativePairPtrSize(std::atomic<NativeDexCachePair<T>>* pair_array,
                                                     size_t idx,
                                                     PointerSize ptr_size) {
  if (ptr_size == PointerSize::k64) {
    auto* array = reinterpret_cast<std::atomic<ConversionPair64>*>(pair_array);
    ConversionPair64 value = AtomicLoadRelaxed16B(&array[idx]);
    return NativeDexCachePair<T>(reinterpret_cast64<T*>(value.first),
                                 dchecked_integral_cast<size_t>(value.second));
  } else {
    auto* array = reinterpret_cast<std::atomic<ConversionPair32>*>(pair_array);
    ConversionPair32 value = array[idx].load(std::memory_order_relaxed);
    return NativeDexCachePair<T>(reinterpret_cast<T*>(value.first), value.second);
  }
}

template <typename T>
void DexCache::SetNativePairPtrSize(std::atomic<NativeDexCachePair<T>>* pair_array,
                                    size_t idx,
                                    NativeDexCachePair<T> pair,
                                    PointerSize ptr_size) {
  if (ptr_size == PointerSize::k64) {
    auto* array = reinterpret_cast<std::atomic<ConversionPair64>*>(pair_array);
    ConversionPair64 v(reinterpret_cast64<uint64_t>(pair.object), pair.index);
    AtomicStoreRelease16B(&array[idx], v);
  } else {
    auto* array = reinterpret_cast<std::atomic<ConversionPair32>*>(pair_array);
    ConversionPair32 v(
        dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(pair.object)),
        dchecked_integral_cast<uint32_t>(pair.index));
    array[idx].store(v, std::memory_order_release);
  }
}

template <typename T,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void VisitDexCachePairs(std::atomic<DexCachePair<T>>* pairs,
                               size_t num_pairs,
                               const Visitor& visitor)
    REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
  for (size_t i = 0; i < num_pairs; ++i) {
    DexCachePair<T> source = pairs[i].load(std::memory_order_relaxed);
    // NOTE: We need the "template" keyword here to avoid a compilation
    // failure. GcRoot<T> is a template argument-dependent type and we need to
    // tell the compiler to treat "Read" as a template rather than a field or
    // function. Otherwise, on encountering the "<" token, the compiler would
    // treat "Read" as a field.
    T* const before = source.object.template Read<kReadBarrierOption>();
    visitor.VisitRootIfNonNull(source.object.AddressWithoutBarrier());
    if (source.object.template Read<kReadBarrierOption>() != before) {
      pairs[i].store(source, std::memory_order_relaxed);
    }
  }
}

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void DexCache::VisitReferences(ObjPtr<Class> klass, const Visitor& visitor) {
  // Visit instance fields first.
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
  // Visit arrays after.
  if (kVisitNativeRoots) {
    VisitDexCachePairs<String, kReadBarrierOption, Visitor>(
        GetStrings(), NumStrings(), visitor);

    VisitDexCachePairs<Class, kReadBarrierOption, Visitor>(
        GetResolvedTypes(), NumResolvedTypes(), visitor);

    VisitDexCachePairs<MethodType, kReadBarrierOption, Visitor>(
        GetResolvedMethodTypes(), NumResolvedMethodTypes(), visitor);

    GcRoot<mirror::CallSite>* resolved_call_sites = GetResolvedCallSites();
    for (size_t i = 0, num_call_sites = NumResolvedCallSites(); i != num_call_sites; ++i) {
      visitor.VisitRootIfNonNull(resolved_call_sites[i].AddressWithoutBarrier());
    }
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void DexCache::FixupStrings(StringDexCacheType* dest, const Visitor& visitor) {
  StringDexCacheType* src = GetStrings();
  for (size_t i = 0, count = NumStrings(); i < count; ++i) {
    StringDexCachePair source = src[i].load(std::memory_order_relaxed);
    String* ptr = source.object.Read<kReadBarrierOption>();
    String* new_source = visitor(ptr);
    source.object = GcRoot<String>(new_source);
    dest[i].store(source, std::memory_order_relaxed);
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void DexCache::FixupResolvedTypes(TypeDexCacheType* dest, const Visitor& visitor) {
  TypeDexCacheType* src = GetResolvedTypes();
  for (size_t i = 0, count = NumResolvedTypes(); i < count; ++i) {
    TypeDexCachePair source = src[i].load(std::memory_order_relaxed);
    Class* ptr = source.object.Read<kReadBarrierOption>();
    Class* new_source = visitor(ptr);
    source.object = GcRoot<Class>(new_source);
    dest[i].store(source, std::memory_order_relaxed);
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void DexCache::FixupResolvedMethodTypes(MethodTypeDexCacheType* dest,
                                               const Visitor& visitor) {
  MethodTypeDexCacheType* src = GetResolvedMethodTypes();
  for (size_t i = 0, count = NumResolvedMethodTypes(); i < count; ++i) {
    MethodTypeDexCachePair source = src[i].load(std::memory_order_relaxed);
    MethodType* ptr = source.object.Read<kReadBarrierOption>();
    MethodType* new_source = visitor(ptr);
    source.object = GcRoot<MethodType>(new_source);
    dest[i].store(source, std::memory_order_relaxed);
  }
}

template <ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void DexCache::FixupResolvedCallSites(GcRoot<mirror::CallSite>* dest,
                                             const Visitor& visitor) {
  GcRoot<mirror::CallSite>* src = GetResolvedCallSites();
  for (size_t i = 0, count = NumResolvedCallSites(); i < count; ++i) {
    mirror::CallSite* source = src[i].Read<kReadBarrierOption>();
    mirror::CallSite* new_source = visitor(source);
    dest[i] = GcRoot<mirror::CallSite>(new_source);
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_DEX_CACHE_INL_H_
