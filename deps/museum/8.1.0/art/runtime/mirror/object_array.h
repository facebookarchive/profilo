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

#ifndef ART_RUNTIME_MIRROR_OBJECT_ARRAY_H_
#define ART_RUNTIME_MIRROR_OBJECT_ARRAY_H_

#include "array.h"
#include "obj_ptr.h"

namespace art {
namespace mirror {

template<class T>
class MANAGED ObjectArray: public Array {
 public:
  // The size of Object[].class.
  static uint32_t ClassSize(PointerSize pointer_size) {
    return Array::ClassSize(pointer_size);
  }

  static ObjectArray<T>* Alloc(Thread* self,
                               ObjPtr<Class> object_array_class,
                               int32_t length,
                               gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjectArray<T>* Alloc(Thread* self,
                               ObjPtr<Class> object_array_class,
                               int32_t length)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE T* Get(int32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if the object can be stored into the array. If not, throws
  // an ArrayStoreException and returns false.
  // TODO fix thread safety analysis: should be REQUIRES_SHARED(Locks::mutator_lock_).
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CheckAssignable(ObjPtr<T> object) NO_THREAD_SAFETY_ANALYSIS;

  ALWAYS_INLINE void Set(int32_t i, ObjPtr<T> object) REQUIRES_SHARED(Locks::mutator_lock_);
  // TODO fix thread safety analysis: should be REQUIRES_SHARED(Locks::mutator_lock_).
  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void Set(int32_t i, ObjPtr<T> object) NO_THREAD_SAFETY_ANALYSIS;

  // Set element without bound and element type checks, to be used in limited
  // circumstances, such as during boot image writing.
  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetWithoutChecks(int32_t i, ObjPtr<T> object) NO_THREAD_SAFETY_ANALYSIS;
  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetWithoutChecksAndWriteBarrier(int32_t i, ObjPtr<T> object)
      NO_THREAD_SAFETY_ANALYSIS;

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE T* GetWithoutChecks(int32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  // Copy src into this array (dealing with overlaps as memmove does) without assignability checks.
  void AssignableMemmove(int32_t dst_pos,
                         ObjPtr<ObjectArray<T>> src,
                         int32_t src_pos,
                         int32_t count)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Copy src into this array assuming no overlap and without assignability checks.
  void AssignableMemcpy(int32_t dst_pos,
                        ObjPtr<ObjectArray<T>> src,
                        int32_t src_pos,
                        int32_t count)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Copy src into this array with assignability checks.
  template<bool kTransactionActive>
  void AssignableCheckingMemcpy(int32_t dst_pos,
                                ObjPtr<ObjectArray<T>> src,
                                int32_t src_pos,
                                int32_t count,
                                bool throw_exception)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ObjectArray<T>* CopyOf(Thread* self, int32_t new_length)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  static MemberOffset OffsetOfElement(int32_t i);

 private:
  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template<typename Visitor>
  void VisitReferences(const Visitor& visitor) NO_THREAD_SAFETY_ANALYSIS;

  friend class Object;  // For VisitReferences
  DISALLOW_IMPLICIT_CONSTRUCTORS(ObjectArray);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_ARRAY_H_
