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

#ifndef ART_RUNTIME_CLASS_LINKER_INL_H_
#define ART_RUNTIME_CLASS_LINKER_INL_H_

#include "art_field.h"
#include "class_linker.h"
#include "gc_root-inl.h"
#include "gc/heap-inl.h"
#include "obj_ptr-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/iftable.h"
#include "mirror/object_array-inl.h"
#include "handle_scope-inl.h"
#include "scoped_thread_state_change-inl.h"

#include <atomic>

namespace art {

inline mirror::Class* ClassLinker::FindArrayClass(Thread* self,
                                                  ObjPtr<mirror::Class>* element_class) {
  for (size_t i = 0; i < kFindArrayCacheSize; ++i) {
    // Read the cached array class once to avoid races with other threads setting it.
    ObjPtr<mirror::Class> array_class = find_array_class_cache_[i].Read();
    if (array_class != nullptr && array_class->GetComponentType() == *element_class) {
      return array_class.Ptr();
    }
  }
  std::string descriptor = "[";
  std::string temp;
  descriptor += (*element_class)->GetDescriptor(&temp);
  StackHandleScope<2> hs(Thread::Current());
  Handle<mirror::ClassLoader> class_loader(hs.NewHandle((*element_class)->GetClassLoader()));
  HandleWrapperObjPtr<mirror::Class> h_element_class(hs.NewHandleWrapper(element_class));
  ObjPtr<mirror::Class> array_class = FindClass(self, descriptor.c_str(), class_loader);
  if (array_class != nullptr) {
    // Benign races in storing array class and incrementing index.
    size_t victim_index = find_array_class_cache_next_victim_;
    find_array_class_cache_[victim_index] = GcRoot<mirror::Class>(array_class);
    find_array_class_cache_next_victim_ = (victim_index + 1) % kFindArrayCacheSize;
  } else {
    // We should have a NoClassDefFoundError.
    self->AssertPendingException();
  }
  return array_class.Ptr();
}

inline ObjPtr<mirror::Class> ClassLinker::LookupResolvedType(
    dex::TypeIndex type_idx,
    ObjPtr<mirror::DexCache> dex_cache,
    ObjPtr<mirror::ClassLoader> class_loader) {
  ObjPtr<mirror::Class> type = dex_cache->GetResolvedType(type_idx);
  if (type == nullptr) {
    type = Runtime::Current()->GetClassLinker()->LookupResolvedType(
        *dex_cache->GetDexFile(), type_idx, dex_cache, class_loader);
  }
  return type;
}

inline mirror::Class* ClassLinker::ResolveType(dex::TypeIndex type_idx, ArtMethod* referrer) {
  Thread::PoisonObjectPointersIfDebug();
  if (kIsDebugBuild) {
    Thread::Current()->AssertNoPendingException();
  }
  ObjPtr<mirror::Class> resolved_type = referrer->GetDexCache()->GetResolvedType(type_idx);
  if (UNLIKELY(resolved_type == nullptr)) {
    StackHandleScope<2> hs(Thread::Current());
    ObjPtr<mirror::Class> declaring_class = referrer->GetDeclaringClass();
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(referrer->GetDexCache()));
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(declaring_class->GetClassLoader()));
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_type = ResolveType(dex_file, type_idx, dex_cache, class_loader);
  }
  return resolved_type.Ptr();
}

inline ArtMethod* ClassLinker::GetResolvedMethod(uint32_t method_idx, ArtMethod* referrer) {
  ArtMethod* resolved_method = referrer->GetDexCacheResolvedMethod(method_idx, image_pointer_size_);
  if (resolved_method == nullptr || resolved_method->IsRuntimeMethod()) {
    return nullptr;
  }
  return resolved_method;
}

inline mirror::Class* ClassLinker::ResolveReferencedClassOfMethod(
    uint32_t method_idx,
    Handle<mirror::DexCache> dex_cache,
    Handle<mirror::ClassLoader> class_loader) {
  // NB: We cannot simply use `GetResolvedMethod(method_idx, ...)->GetDeclaringClass()`. This is
  // because if we did so than an invoke-super could be incorrectly dispatched in cases where
  // GetMethodId(method_idx).class_idx_ refers to a non-interface, non-direct-superclass
  // (super*-class?) of the referrer and the direct superclass of the referrer contains a concrete
  // implementation of the method. If this class's implementation of the method is copied from an
  // interface (either miranda, default or conflict) we would incorrectly assume that is what we
  // want to invoke on, instead of the 'concrete' implementation that the direct superclass
  // contains.
  const DexFile* dex_file = dex_cache->GetDexFile();
  const DexFile::MethodId& method = dex_file->GetMethodId(method_idx);
  ObjPtr<mirror::Class> resolved_type = dex_cache->GetResolvedType(method.class_idx_);
  if (UNLIKELY(resolved_type == nullptr)) {
    resolved_type = ResolveType(*dex_file, method.class_idx_, dex_cache, class_loader);
  }
  return resolved_type.Ptr();
}

template <ClassLinker::ResolveMode kResolveMode>
inline ArtMethod* ClassLinker::ResolveMethod(Thread* self,
                                             uint32_t method_idx,
                                             ArtMethod* referrer,
                                             InvokeType type) {
  ArtMethod* resolved_method = GetResolvedMethod(method_idx, referrer);
  Thread::PoisonObjectPointersIfDebug();
  if (UNLIKELY(resolved_method == nullptr)) {
    ObjPtr<mirror::Class> declaring_class = referrer->GetDeclaringClass();
    StackHandleScope<2> hs(self);
    Handle<mirror::DexCache> h_dex_cache(hs.NewHandle(referrer->GetDexCache()));
    Handle<mirror::ClassLoader> h_class_loader(hs.NewHandle(declaring_class->GetClassLoader()));
    const DexFile* dex_file = h_dex_cache->GetDexFile();
    resolved_method = ResolveMethod<kResolveMode>(*dex_file,
                                                  method_idx,
                                                  h_dex_cache,
                                                  h_class_loader,
                                                  referrer,
                                                  type);
  }
  // Note: We cannot check here to see whether we added the method to the cache. It
  //       might be an erroneous class, which results in it being hidden from us.
  return resolved_method;
}

inline ArtField* ClassLinker::LookupResolvedField(uint32_t field_idx,
                                                  ArtMethod* referrer,
                                                  bool is_static) {
  ObjPtr<mirror::DexCache> dex_cache = referrer->GetDexCache();
  ArtField* field = dex_cache->GetResolvedField(field_idx, image_pointer_size_);
  if (field == nullptr) {
    field = LookupResolvedField(field_idx, dex_cache, referrer->GetClassLoader(), is_static);
  }
  return field;
}

inline ArtField* ClassLinker::ResolveField(uint32_t field_idx,
                                           ArtMethod* referrer,
                                           bool is_static) {
  Thread::PoisonObjectPointersIfDebug();
  ObjPtr<mirror::Class> declaring_class = referrer->GetDeclaringClass();
  ArtField* resolved_field =
      referrer->GetDexCache()->GetResolvedField(field_idx, image_pointer_size_);
  if (UNLIKELY(resolved_field == nullptr)) {
    StackHandleScope<2> hs(Thread::Current());
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(referrer->GetDexCache()));
    Handle<mirror::ClassLoader> class_loader(hs.NewHandle(declaring_class->GetClassLoader()));
    const DexFile& dex_file = *dex_cache->GetDexFile();
    resolved_field = ResolveField(dex_file, field_idx, dex_cache, class_loader, is_static);
    // Note: We cannot check here to see whether we added the field to the cache. The type
    //       might be an erroneous class, which results in it being hidden from us.
  }
  return resolved_field;
}

inline mirror::Class* ClassLinker::GetClassRoot(ClassRoot class_root) {
  DCHECK(!class_roots_.IsNull());
  mirror::ObjectArray<mirror::Class>* class_roots = class_roots_.Read();
  ObjPtr<mirror::Class> klass = class_roots->Get(class_root);
  DCHECK(klass != nullptr);
  return klass.Ptr();
}

template<ReadBarrierOption kReadBarrierOption>
ArtMethod* ClassLinker::FindMethodForProxy(ObjPtr<mirror::Class> proxy_class,
                                           ArtMethod* proxy_method) {
  DCHECK(proxy_class->IsProxyClass());
  DCHECK(proxy_method->IsProxyMethod());
  {
    Thread* const self = Thread::Current();
    ReaderMutexLock mu(self, *Locks::dex_lock_);
    // Locate the dex cache of the original interface/Object
    for (const DexCacheData& data : dex_caches_) {
      if (!self->IsJWeakCleared(data.weak_root) &&
          proxy_method->HasSameDexCacheResolvedMethods(data.resolved_methods,
                                                       image_pointer_size_)) {
        ObjPtr<mirror::DexCache> dex_cache =
            ObjPtr<mirror::DexCache>::DownCast(self->DecodeJObject(data.weak_root));
        if (dex_cache != nullptr) {
          ArtMethod* resolved_method = dex_cache->GetResolvedMethod(
              proxy_method->GetDexMethodIndex(), image_pointer_size_);
          CHECK(resolved_method != nullptr);
          return resolved_method;
        }
      }
    }
  }
  LOG(FATAL) << "Didn't find dex cache for " << proxy_class->PrettyClass() << " "
      << proxy_method->PrettyMethod();
  UNREACHABLE();
}

}  // namespace art

#endif  // ART_RUNTIME_CLASS_LINKER_INL_H_
