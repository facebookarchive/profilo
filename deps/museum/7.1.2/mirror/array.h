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

#ifndef ART_RUNTIME_MIRROR_ARRAY_H_
#define ART_RUNTIME_MIRROR_ARRAY_H_

#include "gc_root.h"
#include "gc/allocator_type.h"
#include "object.h"
#include "object_callbacks.h"

namespace art {

template<class T> class Handle;

namespace mirror {

class MANAGED Array : public Object {
 public:
  // The size of a java.lang.Class representing an array.
  static uint32_t ClassSize(size_t pointer_size);

  // Allocates an array with the given properties, if kFillUsable is true the array will be of at
  // least component_count size, however, if there's usable space at the end of the allocation the
  // array will fill it.
  template <bool kIsInstrumented, bool kFillUsable = false>
  ALWAYS_INLINE static Array* Alloc(Thread* self, Class* array_class, int32_t component_count,
                                    size_t component_size_shift, gc::AllocatorType allocator_type)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static Array* CreateMultiArray(Thread* self, Handle<Class> element_class,
                                 Handle<IntArray> dimensions)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  size_t SizeOf() SHARED_REQUIRES(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int32_t GetLength() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Array, length_));
  }

  void SetLength(int32_t length) SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK_GE(length, 0);
    // We use non transactional version since we can't undo this write. We also disable checking
    // since it would fail during a transaction.
    SetField32<false, false, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Array, length_), length);
  }

  static MemberOffset LengthOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Array, length_);
  }

  static MemberOffset DataOffset(size_t component_size);

  void* GetRawData(size_t component_size, int32_t index)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(component_size).Int32Value() +
        + (index * component_size);
    return reinterpret_cast<void*>(data);
  }

  const void* GetRawData(size_t component_size, int32_t index) const {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(component_size).Int32Value() +
        + (index * component_size);
    return reinterpret_cast<void*>(data);
  }

  // Returns true if the index is valid. If not, throws an ArrayIndexOutOfBoundsException and
  // returns false.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool CheckIsValidIndex(int32_t index) SHARED_REQUIRES(Locks::mutator_lock_);

  Array* CopyOf(Thread* self, int32_t new_length) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

 protected:
  void ThrowArrayStoreException(Object* object) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

 private:
  void ThrowArrayIndexOutOfBoundsException(int32_t index)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // The number of array elements.
  int32_t length_;
  // Marker for the data (used by generated code)
  uint32_t first_element_[0];

  DISALLOW_IMPLICIT_CONSTRUCTORS(Array);
};

template<typename T>
class MANAGED PrimitiveArray : public Array {
 public:
  typedef T ElementType;

  static PrimitiveArray<T>* Alloc(Thread* self, size_t length)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  const T* GetData() const ALWAYS_INLINE  SHARED_REQUIRES(Locks::mutator_lock_) {
    return reinterpret_cast<const T*>(GetRawData(sizeof(T), 0));
  }

  T* GetData() ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    return reinterpret_cast<T*>(GetRawData(sizeof(T), 0));
  }

  T Get(int32_t i) ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_);

  T GetWithoutChecks(int32_t i) ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(CheckIsValidIndex(i)) << "i=" << i << " length=" << GetLength();
    return GetData()[i];
  }

  void Set(int32_t i, T value) ALWAYS_INLINE SHARED_REQUIRES(Locks::mutator_lock_);

  // TODO fix thread safety analysis broken by the use of template. This should be
  // SHARED_REQUIRES(Locks::mutator_lock_).
  template<bool kTransactionActive, bool kCheckTransaction = true>
  void Set(int32_t i, T value) ALWAYS_INLINE NO_THREAD_SAFETY_ANALYSIS;

  // TODO fix thread safety analysis broken by the use of template. This should be
  // SHARED_REQUIRES(Locks::mutator_lock_).
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetWithoutChecks(int32_t i, T value) ALWAYS_INLINE NO_THREAD_SAFETY_ANALYSIS;

  /*
   * Works like memmove(), except we guarantee not to allow tearing of array values (ie using
   * smaller than element size copies). Arguments are assumed to be within the bounds of the array
   * and the arrays non-null.
   */
  void Memmove(int32_t dst_pos, PrimitiveArray<T>* src, int32_t src_pos, int32_t count)
      SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * Works like memcpy(), except we guarantee not to allow tearing of array values (ie using
   * smaller than element size copies). Arguments are assumed to be within the bounds of the array
   * and the arrays non-null.
   */
  void Memcpy(int32_t dst_pos, PrimitiveArray<T>* src, int32_t src_pos, int32_t count)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static void SetArrayClass(Class* array_class) {
    CHECK(array_class_.IsNull());
    CHECK(array_class != nullptr);
    array_class_ = GcRoot<Class>(array_class);
  }

  static Class* GetArrayClass() SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(!array_class_.IsNull());
    return array_class_.Read();
  }

  static void ResetArrayClass() {
    CHECK(!array_class_.IsNull());
    array_class_ = GcRoot<Class>(nullptr);
  }

  static void VisitRoots(RootVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  static GcRoot<Class> array_class_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PrimitiveArray);
};

// Either an IntArray or a LongArray.
class PointerArray : public Array {
 public:
  template<typename T,
           VerifyObjectFlags kVerifyFlags = kVerifyNone,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  T GetElementPtrSize(uint32_t idx, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  template<bool kTransactionActive = false, bool kUnchecked = false>
  void SetElementPtrSize(uint32_t idx, uint64_t element, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  template<bool kTransactionActive = false, bool kUnchecked = false, typename T>
  void SetElementPtrSize(uint32_t idx, T* element, size_t ptr_size)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Fixup the pointers in the dest arrays by passing our pointers through the visitor. Only copies
  // to dest if visitor(source_ptr) != source_ptr.
  template <VerifyObjectFlags kVerifyFlags = kVerifyNone,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor>
  void Fixup(mirror::PointerArray* dest, size_t pointer_size, const Visitor& visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ARRAY_H_
