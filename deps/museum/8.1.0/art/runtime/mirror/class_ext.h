/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_CLASS_EXT_H_
#define ART_RUNTIME_MIRROR_CLASS_EXT_H_

#include "array.h"
#include "class.h"
#include "dex_cache.h"
#include "gc_root.h"
#include "object.h"
#include "object_array.h"
#include "string.h"

namespace art {

struct ClassExtOffsets;

namespace mirror {

// C++ mirror of dalvik.system.ClassExt
class MANAGED ClassExt : public Object {
 public:
  static uint32_t ClassSize(PointerSize pointer_size);

  // Size of an instance of dalvik.system.ClassExt.
  static constexpr uint32_t InstanceSize() {
    return sizeof(ClassExt);
  }

  void SetVerifyError(ObjPtr<Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  Object* GetVerifyError() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<ClassExt>(OFFSET_OF_OBJECT_MEMBER(ClassExt, verify_error_));
  }

  ObjectArray<DexCache>* GetObsoleteDexCaches() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<ObjectArray<DexCache>>(
        OFFSET_OF_OBJECT_MEMBER(ClassExt, obsolete_dex_caches_));
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  inline PointerArray* GetObsoleteMethods() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<PointerArray, kVerifyFlags, kReadBarrierOption>(
        OFFSET_OF_OBJECT_MEMBER(ClassExt, obsolete_methods_));
  }

  Object* GetOriginalDexFile() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<Object>(OFFSET_OF_OBJECT_MEMBER(ClassExt, original_dex_file_));
  }

  void SetOriginalDexFile(ObjPtr<Object> bytes) REQUIRES_SHARED(Locks::mutator_lock_);

  void SetObsoleteArrays(ObjPtr<PointerArray> methods, ObjPtr<ObjectArray<DexCache>> dex_caches)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Extend the obsolete arrays by the given amount.
  bool ExtendObsoleteArrays(Thread* self, uint32_t increase)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static void SetClass(ObjPtr<Class> dalvik_system_ClassExt);
  static void ResetClass();
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier, class Visitor>
  inline void VisitNativeRoots(Visitor& visitor, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static ClassExt* Alloc(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  HeapReference<ObjectArray<DexCache>> obsolete_dex_caches_;

  HeapReference<PointerArray> obsolete_methods_;

  HeapReference<Object> original_dex_file_;

  // The saved verification error of this class.
  HeapReference<Object> verify_error_;

  static GcRoot<Class> dalvik_system_ClassExt_;

  friend struct art::ClassExtOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(ClassExt);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_EXT_H_
