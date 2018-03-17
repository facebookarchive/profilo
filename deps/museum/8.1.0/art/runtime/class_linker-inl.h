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

template <bool kThrowOnError, typename ClassGetter>
inline bool ClassLinker::CheckInvokeClassMismatch(ObjPtr<mirror::DexCache> dex_cache,
                                                  InvokeType type,
                                                  ClassGetter class_getter) {
  switch (type) {
    case kStatic:
    case kSuper:
      break;
    case kInterface: {
      // We have to check whether the method id really belongs to an interface (dex static bytecode
      // constraints A15, A16). Otherwise you must not invoke-interface on it.
      ObjPtr<mirror::Class> klass = class_getter();
      if (UNLIKELY(!klass->IsInterface())) {
        if (kThrowOnError) {
          ThrowIncompatibleClassChangeError(klass,
                                            "Found class %s, but interface was expected",
                                            klass->PrettyDescriptor().c_str());
        }
        return true;
      }
      break;
    }
    case kDirect:
      if (dex_cache->GetDexFile()->GetVersion() >= DexFile::kDefaultMethodsVersion) {
        break;
      }
      FALLTHROUGH_INTENDED;
    case kVirtual: {
      // Similarly, invoke-virtual (and invoke-direct without default methods) must reference
      // a non-interface class (dex static bytecode constraint A24, A25).
      ObjPtr<mirror::Class> klass = class_getter();
      if (UNLIKELY(klass->IsInterface())) {
        if (kThrowOnError) {
          ThrowIncompatibleClassChangeError(klass,
                                            "Found interface %s, but class was expected",
                                            klass->PrettyDescriptor().c_str());
        }
        return true;
      }
      break;
    }
    default:
      LOG(FATAL) << "Unreachable - invocation type: " << type;
      UNREACHABLE();
  }
  return false;
}

template <bool kThrow>
inline bool ClassLinker::CheckInvokeClassMismatch(ObjPtr<mirror::DexCache> dex_cache,
                                                  InvokeType type,
                                                  uint32_t method_idx,
                                                  ObjPtr<mirror::ClassLoader> class_loader) {
  return CheckInvokeClassMismatch<kThrow>(
      dex_cache,
      type,
      [this, dex_cache, method_idx, class_loader]() REQUIRES_SHARED(Locks::mutator_lock_) {
        const DexFile& dex_file = *dex_cache->GetDexFile();
        const DexFile::MethodId& method_id = dex_file.GetMethodId(method_idx);
        ObjPtr<mirror::Class> klass =
            LookupResolvedType(dex_file, method_id.class_idx_, dex_cache, class_loader);
        DCHECK(klass != nullptr);
        return klass;
      });
}

inline ArtMethod* ClassLinker::LookupResolvedMethod(uint32_t method_idx,
                                                    ObjPtr<mirror::DexCache> dex_cache,
                                                    ObjPtr<mirror::ClassLoader> class_loader) {
  PointerSize pointer_size = image_pointer_size_;
  ArtMethod* resolved = dex_cache->GetResolvedMethod(method_idx, pointer_size);
  if (resolved == nullptr) {
    const DexFile& dex_file = *dex_cache->GetDexFile();
    const DexFile::MethodId& method_id = dex_file.GetMethodId(method_idx);
    ObjPtr<mirror::Class> klass = LookupResolvedType(method_id.class_idx_, dex_cache, class_loader);
    if (klass != nullptr) {
      if (klass->IsInterface()) {
        resolved = klass->FindInterfaceMethod(dex_cache, method_idx, pointer_size);
      } else {
        resolved = klass->FindClassMethod(dex_cache, method_idx, pointer_size);
      }
      if (resolved != nullptr) {
        dex_cache->SetResolvedMethod(method_idx, resolved, pointer_size);
      }
    }
  }
  return resolved;
}

template <InvokeType type, ClassLinker::ResolveMode kResolveMode>
inline ArtMethod* ClassLinker::GetResolvedMethod(uint32_t method_idx, ArtMethod* referrer) {
  DCHECK(referrer != nullptr);
  // Note: The referrer can be a Proxy constructor. In that case, we need to do the
  // lookup in the context of the original method from where it steals the code.
  // However, we delay the GetInterfaceMethodIfProxy() until needed.
  DCHECK(!referrer->IsProxyMethod() || referrer->IsConstructor());
  ArtMethod* resolved_method = referrer->GetDexCacheResolvedMethod(method_idx, image_pointer_size_);
  if (resolved_method == nullptr) {
    return nullptr;
  }
  DCHECK(!resolved_method->IsRuntimeMethod());
  if (kResolveMode == ResolveMode::kCheckICCEAndIAE) {
    referrer = referrer->GetInterfaceMethodIfProxy(image_pointer_size_);
    // Check if the invoke type matches the class type.
    ObjPtr<mirror::DexCache> dex_cache = referrer->GetDexCache();
    ObjPtr<mirror::ClassLoader> class_loader = referrer->GetClassLoader();
    if (CheckInvokeClassMismatch</* kThrow */ false>(dex_cache, type, method_idx, class_loader)) {
      return nullptr;
    }
    // Check access.
    ObjPtr<mirror::Class> referring_class = referrer->GetDeclaringClass();
    if (!referring_class->CanAccessResolvedMethod(resolved_method->GetDeclaringClass(),
                                                  resolved_method,
                                                  dex_cache,
                                                  method_idx)) {
      return nullptr;
    }
    // Check if the invoke type matches the method type.
    if (UNLIKELY(resolved_method->CheckIncompatibleClassChange(type))) {
      return nullptr;
    }
  }
  return resolved_method;
}

template <ClassLinker::ResolveMode kResolveMode>
inline ArtMethod* ClassLinker::ResolveMethod(Thread* self,
                                             uint32_t method_idx,
                                             ArtMethod* referrer,
                                             InvokeType type) {
  DCHECK(referrer != nullptr);
  // Note: The referrer can be a Proxy constructor. In that case, we need to do the
  // lookup in the context of the original method from where it steals the code.
  // However, we delay the GetInterfaceMethodIfProxy() until needed.
  DCHECK(!referrer->IsProxyMethod() || referrer->IsConstructor());
  Thread::PoisonObjectPointersIfDebug();
  ArtMethod* resolved_method = referrer->GetDexCacheResolvedMethod(method_idx, image_pointer_size_);
  DCHECK(resolved_method == nullptr || !resolved_method->IsRuntimeMethod());
  if (UNLIKELY(resolved_method == nullptr)) {
    referrer = referrer->GetInterfaceMethodIfProxy(image_pointer_size_);
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
  } else if (kResolveMode == ResolveMode::kCheckICCEAndIAE) {
    referrer = referrer->GetInterfaceMethodIfProxy(image_pointer_size_);
    // Check if the invoke type matches the class type.
    ObjPtr<mirror::DexCache> dex_cache = referrer->GetDexCache();
    ObjPtr<mirror::ClassLoader> class_loader = referrer->GetClassLoader();
    if (CheckInvokeClassMismatch</* kThrow */ true>(dex_cache, type, method_idx, class_loader)) {
      DCHECK(Thread::Current()->IsExceptionPending());
      return nullptr;
    }
    // Check access.
    ObjPtr<mirror::Class> referring_class = referrer->GetDeclaringClass();
    if (!referring_class->CheckResolvedMethodAccess(resolved_method->GetDeclaringClass(),
                                                    resolved_method,
                                                    dex_cache,
                                                    method_idx,
                                                    type)) {
      DCHECK(Thread::Current()->IsExceptionPending());
      return nullptr;
    }
    // Check if the invoke type matches the method type.
    if (UNLIKELY(resolved_method->CheckIncompatibleClassChange(type))) {
      ThrowIncompatibleClassChangeError(type,
                                        resolved_method->GetInvokeType(),
                                        resolved_method,
                                        referrer);
      return nullptr;
    }
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

template <class Visitor>
inline void ClassLinker::VisitClassTables(const Visitor& visitor) {
  Thread* const self = Thread::Current();
  WriterMutexLock mu(self, *Locks::classlinker_classes_lock_);
  for (const ClassLoaderData& data : class_loaders_) {
    if (data.class_table != nullptr) {
      visitor(data.class_table);
    }
  }
}

}  // namespace art

#endif  // ART_RUNTIME_CLASS_LINKER_INL_H_
