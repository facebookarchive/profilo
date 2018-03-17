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

#ifndef ART_RUNTIME_MIRROR_ARRAY_INL_H_
#define ART_RUNTIME_MIRROR_ARRAY_INL_H_

#include "array.h"

#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "class-inl.h"
#include "gc/heap-inl.h"
#include "thread.h"

namespace art {
namespace mirror {

inline uint32_t Array::ClassSize(size_t pointer_size) {
  uint32_t vtable_entries = Object::kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline size_t Array::SizeOf() {
  // This is safe from overflow because the array was already allocated, so we know it's sane.
  size_t component_size_shift = GetClass<kVerifyFlags, kReadBarrierOption>()->
      template GetComponentSizeShift<kReadBarrierOption>();
  // Don't need to check this since we already check this in GetClass.
  int32_t component_count =
      GetLength<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>();
  size_t header_size = DataOffset(1U << component_size_shift).SizeValue();
  size_t data_size = component_count << component_size_shift;
  return header_size + data_size;
}

inline MemberOffset Array::DataOffset(size_t component_size) {
  DCHECK(IsPowerOfTwo(component_size)) << component_size;
  size_t data_offset = RoundUp(OFFSETOF_MEMBER(Array, first_element_), component_size);
  DCHECK_EQ(RoundUp(data_offset, component_size), data_offset)
      << "Array data offset isn't aligned with component size";
  return MemberOffset(data_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Array::CheckIsValidIndex(int32_t index) {
  if (UNLIKELY(static_cast<uint32_t>(index) >=
               static_cast<uint32_t>(GetLength<kVerifyFlags>()))) {
    ThrowArrayIndexOutOfBoundsException(index);
    return false;
  }
  return true;
}

static inline size_t ComputeArraySize(int32_t component_count, size_t component_size_shift) {
  DCHECK_GE(component_count, 0);

  size_t component_size = 1U << component_size_shift;
  size_t header_size = Array::DataOffset(component_size).SizeValue();
  size_t data_size = static_cast<size_t>(component_count) << component_size_shift;
  size_t size = header_size + data_size;

  // Check for size_t overflow if this was an unreasonable request
  // but let the caller throw OutOfMemoryError.
#ifdef __LP64__
  // 64-bit. No overflow as component_count is 32-bit and the maximum
  // component size is 8.
  DCHECK_LE((1U << component_size_shift), 8U);
#else
  // 32-bit.
  DCHECK_NE(header_size, 0U);
  DCHECK_EQ(RoundUp(header_size, component_size), header_size);
  // The array length limit (exclusive).
  const size_t length_limit = (0U - header_size) >> component_size_shift;
  if (UNLIKELY(length_limit <= static_cast<size_t>(component_count))) {
    return 0;  // failure
  }
#endif
  return size;
}

// Used for setting the array length in the allocation code path to ensure it is guarded by a
// StoreStore fence.
class SetLengthVisitor {
 public:
  explicit SetLengthVisitor(int32_t length) : length_(length) {
  }

  void operator()(Object* obj, size_t usable_size) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    UNUSED(usable_size);
    // Avoid AsArray as object is not yet in live bitmap or allocation stack.
    Array* array = down_cast<Array*>(obj);
    // DCHECK(array->IsArrayInstance());
    array->SetLength(length_);
  }

 private:
  const int32_t length_;

  DISALLOW_COPY_AND_ASSIGN(SetLengthVisitor);
};

// Similar to SetLengthVisitor, used for setting the array length to fill the usable size of an
// array.
class SetLengthToUsableSizeVisitor {
 public:
  SetLengthToUsableSizeVisitor(int32_t min_length, size_t header_size,
                               size_t component_size_shift) :
      minimum_length_(min_length), header_size_(header_size),
      component_size_shift_(component_size_shift) {
  }

  void operator()(Object* obj, size_t usable_size) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Avoid AsArray as object is not yet in live bitmap or allocation stack.
    Array* array = down_cast<Array*>(obj);
    // DCHECK(array->IsArrayInstance());
    int32_t length = (usable_size - header_size_) >> component_size_shift_;
    DCHECK_GE(length, minimum_length_);
    uint8_t* old_end = reinterpret_cast<uint8_t*>(array->GetRawData(1U << component_size_shift_,
                                                                    minimum_length_));
    uint8_t* new_end = reinterpret_cast<uint8_t*>(array->GetRawData(1U << component_size_shift_,
                                                                    length));
    // Ensure space beyond original allocation is zeroed.
    memset(old_end, 0, new_end - old_end);
    array->SetLength(length);
  }

 private:
  const int32_t minimum_length_;
  const size_t header_size_;
  const size_t component_size_shift_;

  DISALLOW_COPY_AND_ASSIGN(SetLengthToUsableSizeVisitor);
};

template <bool kIsInstrumented, bool kFillUsable>
inline Array* Array::Alloc(Thread* self, Class* array_class, int32_t component_count,
                           size_t component_size_shift, gc::AllocatorType allocator_type) {
  DCHECK(allocator_type != gc::kAllocatorTypeLOS);
  DCHECK(array_class != nullptr);
  DCHECK(array_class->IsArrayClass());
  DCHECK_EQ(array_class->GetComponentSizeShift(), component_size_shift);
  DCHECK_EQ(array_class->GetComponentSize(), (1U << component_size_shift));
  size_t size = ComputeArraySize(component_count, component_size_shift);
#ifdef __LP64__
  // 64-bit. No size_t overflow.
  DCHECK_NE(size, 0U);
#else
  // 32-bit.
  if (UNLIKELY(size == 0)) {
    self->ThrowOutOfMemoryError(StringPrintf("%s of length %d would overflow",
                                             PrettyDescriptor(array_class).c_str(),
                                             component_count).c_str());
    return nullptr;
  }
#endif
  gc::Heap* heap = Runtime::Current()->GetHeap();
  Array* result;
  if (!kFillUsable) {
    SetLengthVisitor visitor(component_count);
    result = down_cast<Array*>(
        heap->AllocObjectWithAllocator<kIsInstrumented, true>(self, array_class, size,
                                                              allocator_type, visitor));
  } else {
    SetLengthToUsableSizeVisitor visitor(component_count,
                                         DataOffset(1U << component_size_shift).SizeValue(),
                                         component_size_shift);
    result = down_cast<Array*>(
        heap->AllocObjectWithAllocator<kIsInstrumented, true>(self, array_class, size,
                                                              allocator_type, visitor));
  }
  if (kIsDebugBuild && result != nullptr && Runtime::Current()->IsStarted()) {
    array_class = result->GetClass();  // In case the array class moved.
    CHECK_EQ(array_class->GetComponentSize(), 1U << component_size_shift);
    if (!kFillUsable) {
      CHECK_EQ(result->SizeOf(), size);
    } else {
      CHECK_GE(result->SizeOf(), size);
    }
  }
  return result;
}

template<class T>
inline void PrimitiveArray<T>::VisitRoots(RootVisitor* visitor) {
  array_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

template<typename T>
inline PrimitiveArray<T>* PrimitiveArray<T>::Alloc(Thread* self, size_t length) {
  Array* raw_array = Array::Alloc<true>(self, GetArrayClass(), length,
                                        ComponentSizeShiftWidth(sizeof(T)),
                                        Runtime::Current()->GetHeap()->GetCurrentAllocator());
  return down_cast<PrimitiveArray<T>*>(raw_array);
}

template<typename T>
inline T PrimitiveArray<T>::Get(int32_t i) {
  if (!CheckIsValidIndex(i)) {
    DCHECK(Thread::Current()->IsExceptionPending());
    return T(0);
  }
  return GetWithoutChecks(i);
}

template<typename T>
inline void PrimitiveArray<T>::Set(int32_t i, T value) {
  if (Runtime::Current()->IsActiveTransaction()) {
    Set<true>(i, value);
  } else {
    Set<false>(i, value);
  }
}

template<typename T>
template<bool kTransactionActive, bool kCheckTransaction>
inline void PrimitiveArray<T>::Set(int32_t i, T value) {
  if (CheckIsValidIndex(i)) {
    SetWithoutChecks<kTransactionActive, kCheckTransaction>(i, value);
  } else {
    DCHECK(Thread::Current()->IsExceptionPending());
  }
}

template<typename T>
template<bool kTransactionActive, bool kCheckTransaction>
inline void PrimitiveArray<T>::SetWithoutChecks(int32_t i, T value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteArray(this, i, GetWithoutChecks(i));
  }
  DCHECK(CheckIsValidIndex(i));
  GetData()[i] = value;
}
// Backward copy where elements are of aligned appropriately for T. Count is in T sized units.
// Copies are guaranteed not to tear when the sizeof T is less-than 64bit.
template<typename T>
static inline void ArrayBackwardCopy(T* d, const T* s, int32_t count) {
  d += count;
  s += count;
  for (int32_t i = 0; i < count; ++i) {
    d--;
    s--;
    *d = *s;
  }
}

// Forward copy where elements are of aligned appropriately for T. Count is in T sized units.
// Copies are guaranteed not to tear when the sizeof T is less-than 64bit.
template<typename T>
static inline void ArrayForwardCopy(T* d, const T* s, int32_t count) {
  for (int32_t i = 0; i < count; ++i) {
    *d = *s;
    d++;
    s++;
  }
}

template<class T>
inline void PrimitiveArray<T>::Memmove(int32_t dst_pos, PrimitiveArray<T>* src, int32_t src_pos,
                                       int32_t count) {
  if (UNLIKELY(count == 0)) {
    return;
  }
  DCHECK_GE(dst_pos, 0);
  DCHECK_GE(src_pos, 0);
  DCHECK_GT(count, 0);
  DCHECK(src != nullptr);
  DCHECK_LT(dst_pos, GetLength());
  DCHECK_LE(dst_pos, GetLength() - count);
  DCHECK_LT(src_pos, src->GetLength());
  DCHECK_LE(src_pos, src->GetLength() - count);

  // Note for non-byte copies we can't rely on standard libc functions like memcpy(3) and memmove(3)
  // in our implementation, because they may copy byte-by-byte.
  if (LIKELY(src != this)) {
    // Memcpy ok for guaranteed non-overlapping distinct arrays.
    Memcpy(dst_pos, src, src_pos, count);
  } else {
    // Handle copies within the same array using the appropriate direction copy.
    void* dst_raw = GetRawData(sizeof(T), dst_pos);
    const void* src_raw = src->GetRawData(sizeof(T), src_pos);
    if (sizeof(T) == sizeof(uint8_t)) {
      uint8_t* d = reinterpret_cast<uint8_t*>(dst_raw);
      const uint8_t* s = reinterpret_cast<const uint8_t*>(src_raw);
      memmove(d, s, count);
    } else {
      const bool copy_forward = (dst_pos < src_pos) || (dst_pos - src_pos >= count);
      if (sizeof(T) == sizeof(uint16_t)) {
        uint16_t* d = reinterpret_cast<uint16_t*>(dst_raw);
        const uint16_t* s = reinterpret_cast<const uint16_t*>(src_raw);
        if (copy_forward) {
          ArrayForwardCopy<uint16_t>(d, s, count);
        } else {
          ArrayBackwardCopy<uint16_t>(d, s, count);
        }
      } else if (sizeof(T) == sizeof(uint32_t)) {
        uint32_t* d = reinterpret_cast<uint32_t*>(dst_raw);
        const uint32_t* s = reinterpret_cast<const uint32_t*>(src_raw);
        if (copy_forward) {
          ArrayForwardCopy<uint32_t>(d, s, count);
        } else {
          ArrayBackwardCopy<uint32_t>(d, s, count);
        }
      } else {
        DCHECK_EQ(sizeof(T), sizeof(uint64_t));
        uint64_t* d = reinterpret_cast<uint64_t*>(dst_raw);
        const uint64_t* s = reinterpret_cast<const uint64_t*>(src_raw);
        if (copy_forward) {
          ArrayForwardCopy<uint64_t>(d, s, count);
        } else {
          ArrayBackwardCopy<uint64_t>(d, s, count);
        }
      }
    }
  }
}

template<class T>
inline void PrimitiveArray<T>::Memcpy(int32_t dst_pos, PrimitiveArray<T>* src, int32_t src_pos,
                                      int32_t count) {
  if (UNLIKELY(count == 0)) {
    return;
  }
  DCHECK_GE(dst_pos, 0);
  DCHECK_GE(src_pos, 0);
  DCHECK_GT(count, 0);
  DCHECK(src != nullptr);
  DCHECK_LT(dst_pos, GetLength());
  DCHECK_LE(dst_pos, GetLength() - count);
  DCHECK_LT(src_pos, src->GetLength());
  DCHECK_LE(src_pos, src->GetLength() - count);

  // Note for non-byte copies we can't rely on standard libc functions like memcpy(3) and memmove(3)
  // in our implementation, because they may copy byte-by-byte.
  void* dst_raw = GetRawData(sizeof(T), dst_pos);
  const void* src_raw = src->GetRawData(sizeof(T), src_pos);
  if (sizeof(T) == sizeof(uint8_t)) {
    memcpy(dst_raw, src_raw, count);
  } else if (sizeof(T) == sizeof(uint16_t)) {
    uint16_t* d = reinterpret_cast<uint16_t*>(dst_raw);
    const uint16_t* s = reinterpret_cast<const uint16_t*>(src_raw);
    ArrayForwardCopy<uint16_t>(d, s, count);
  } else if (sizeof(T) == sizeof(uint32_t)) {
    uint32_t* d = reinterpret_cast<uint32_t*>(dst_raw);
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src_raw);
    ArrayForwardCopy<uint32_t>(d, s, count);
  } else {
    DCHECK_EQ(sizeof(T), sizeof(uint64_t));
    uint64_t* d = reinterpret_cast<uint64_t*>(dst_raw);
    const uint64_t* s = reinterpret_cast<const uint64_t*>(src_raw);
    ArrayForwardCopy<uint64_t>(d, s, count);
  }
}

template<typename T>
inline T PointerArray::GetElementPtrSize(uint32_t idx, size_t ptr_size) {
  // C style casts here since we sometimes have T be a pointer, or sometimes an integer
  // (for stack traces).
  if (ptr_size == 8) {
    return (T)static_cast<uintptr_t>(AsLongArray()->GetWithoutChecks(idx));
  }
  DCHECK_EQ(ptr_size, 4u);
  return (T)static_cast<uintptr_t>(AsIntArray()->GetWithoutChecks(idx));
}

template<bool kTransactionActive, bool kUnchecked, typename T>
inline void PointerArray::SetElementPtrSize(uint32_t idx, T element, size_t ptr_size) {
  if (ptr_size == 8) {
    (kUnchecked ? down_cast<LongArray*>(static_cast<Object*>(this)) : AsLongArray())->
        SetWithoutChecks<kTransactionActive>(idx, (uint64_t)(element));
  } else {
    DCHECK_EQ(ptr_size, 4u);
    DCHECK_LE((uintptr_t)element, 0xFFFFFFFFu);
    (kUnchecked ? down_cast<IntArray*>(static_cast<Object*>(this)) : AsIntArray())
        ->SetWithoutChecks<kTransactionActive>(idx, static_cast<uint32_t>((uintptr_t)element));
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ARRAY_INL_H_
