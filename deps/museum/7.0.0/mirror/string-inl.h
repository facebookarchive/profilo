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

#ifndef ART_RUNTIME_MIRROR_STRING_INL_H_
#define ART_RUNTIME_MIRROR_STRING_INL_H_

#include "array.h"
#include "base/bit_utils.h"
#include "class.h"
#include "gc/heap-inl.h"
#include "globals.h"
#include "intern_table.h"
#include "runtime.h"
#include "string.h"
#include "thread.h"
#include "utf.h"
#include "utils.h"

namespace art {
namespace mirror {

inline uint32_t String::ClassSize(size_t pointer_size) {
  uint32_t vtable_entries = Object::kVTableLength + 56;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 1, 2, pointer_size);
}

// Sets string count in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountVisitor {
 public:
  explicit SetStringCountVisitor(int32_t count) : count_(count) {
  }

  void operator()(Object* obj, size_t usable_size ATTRIBUTE_UNUSED) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    String* string = down_cast<String*>(obj);
    string->SetCount(count_);
  }

 private:
  const int32_t count_;
};

// Sets string count and value in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountAndBytesVisitor {
 public:
  SetStringCountAndBytesVisitor(int32_t count, Handle<ByteArray> src_array, int32_t offset,
                                int32_t high_byte)
      : count_(count), src_array_(src_array), offset_(offset), high_byte_(high_byte) {
  }

  void operator()(Object* obj, size_t usable_size ATTRIBUTE_UNUSED) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    String* string = down_cast<String*>(obj);
    string->SetCount(count_);
    uint16_t* value = string->GetValue();
    const uint8_t* const src = reinterpret_cast<uint8_t*>(src_array_->GetData()) + offset_;
    for (int i = 0; i < count_; i++) {
      value[i] = high_byte_ + (src[i] & 0xFF);
    }
  }

 private:
  const int32_t count_;
  Handle<ByteArray> src_array_;
  const int32_t offset_;
  const int32_t high_byte_;
};

// Sets string count and value in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountAndValueVisitorFromCharArray {
 public:
  SetStringCountAndValueVisitorFromCharArray(int32_t count, Handle<CharArray> src_array,
                                             int32_t offset) :
    count_(count), src_array_(src_array), offset_(offset) {
  }

  void operator()(Object* obj, size_t usable_size ATTRIBUTE_UNUSED) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    String* string = down_cast<String*>(obj);
    string->SetCount(count_);
    const uint16_t* const src = src_array_->GetData() + offset_;
    memcpy(string->GetValue(), src, count_ * sizeof(uint16_t));
  }

 private:
  const int32_t count_;
  Handle<CharArray> src_array_;
  const int32_t offset_;
};

// Sets string count and value in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountAndValueVisitorFromString {
 public:
  SetStringCountAndValueVisitorFromString(int32_t count, Handle<String> src_string,
                                          int32_t offset) :
    count_(count), src_string_(src_string), offset_(offset) {
  }

  void operator()(Object* obj, size_t usable_size ATTRIBUTE_UNUSED) const
      SHARED_REQUIRES(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    String* string = down_cast<String*>(obj);
    string->SetCount(count_);
    const uint16_t* const src = src_string_->GetValue() + offset_;
    memcpy(string->GetValue(), src, count_ * sizeof(uint16_t));
  }

 private:
  const int32_t count_;
  Handle<String> src_string_;
  const int32_t offset_;
};

inline String* String::Intern() {
  return Runtime::Current()->GetInternTable()->InternWeak(this);
}

inline uint16_t String::CharAt(int32_t index) {
  int32_t count = GetField32(OFFSET_OF_OBJECT_MEMBER(String, count_));
  if (UNLIKELY((index < 0) || (index >= count))) {
    Thread* self = Thread::Current();
    self->ThrowNewExceptionF("Ljava/lang/StringIndexOutOfBoundsException;",
                             "length=%i; index=%i", count, index);
    return 0;
  }
  return GetValue()[index];
}

template<VerifyObjectFlags kVerifyFlags>
inline size_t String::SizeOf() {
  size_t size = sizeof(String) + (sizeof(uint16_t) * GetLength<kVerifyFlags>());
  // String.equals() intrinsics assume zero-padding up to kObjectAlignment,
  // so make sure the zero-padding is actually copied around if GC compaction
  // chooses to copy only SizeOf() bytes.
  // http://b/23528461
  return RoundUp(size, kObjectAlignment);
}

template <bool kIsInstrumented, typename PreFenceVisitor>
inline String* String::Alloc(Thread* self, int32_t utf16_length, gc::AllocatorType allocator_type,
                             const PreFenceVisitor& pre_fence_visitor) {
  constexpr size_t header_size = sizeof(String);
  static_assert(sizeof(utf16_length) <= sizeof(size_t),
                "static_cast<size_t>(utf16_length) must not lose bits.");
  size_t length = static_cast<size_t>(utf16_length);
  size_t data_size = sizeof(uint16_t) * length;
  size_t size = header_size + data_size;
  // String.equals() intrinsics assume zero-padding up to kObjectAlignment,
  // so make sure the allocator clears the padding as well.
  // http://b/23528461
  size_t alloc_size = RoundUp(size, kObjectAlignment);
  Class* string_class = GetJavaLangString();

  // Check for overflow and throw OutOfMemoryError if this was an unreasonable request.
  // Do this by comparing with the maximum length that will _not_ cause an overflow.
  constexpr size_t overflow_length = (-header_size) / sizeof(uint16_t);  // Unsigned arithmetic.
  constexpr size_t max_alloc_length = overflow_length - 1u;
  static_assert(IsAligned<sizeof(uint16_t)>(kObjectAlignment),
                "kObjectAlignment must be at least as big as Java char alignment");
  constexpr size_t max_length = RoundDown(max_alloc_length, kObjectAlignment / sizeof(uint16_t));
  if (UNLIKELY(length > max_length)) {
    self->ThrowOutOfMemoryError(StringPrintf("%s of length %d would overflow",
                                             PrettyDescriptor(string_class).c_str(),
                                             utf16_length).c_str());
    return nullptr;
  }

  gc::Heap* heap = Runtime::Current()->GetHeap();
  return down_cast<String*>(
      heap->AllocObjectWithAllocator<kIsInstrumented, true>(self, string_class, alloc_size,
                                                            allocator_type, pre_fence_visitor));
}

template <bool kIsInstrumented>
inline String* String::AllocFromByteArray(Thread* self, int32_t byte_length,
                                          Handle<ByteArray> array, int32_t offset,
                                          int32_t high_byte, gc::AllocatorType allocator_type) {
  SetStringCountAndBytesVisitor visitor(byte_length, array, offset, high_byte << 8);
  String* string = Alloc<kIsInstrumented>(self, byte_length, allocator_type, visitor);
  return string;
}

template <bool kIsInstrumented>
inline String* String::AllocFromCharArray(Thread* self, int32_t count,
                                          Handle<CharArray> array, int32_t offset,
                                          gc::AllocatorType allocator_type) {
  // It is a caller error to have a count less than the actual array's size.
  DCHECK_GE(array->GetLength(), count);
  SetStringCountAndValueVisitorFromCharArray visitor(count, array, offset);
  String* new_string = Alloc<kIsInstrumented>(self, count, allocator_type, visitor);
  return new_string;
}

template <bool kIsInstrumented>
inline String* String::AllocFromString(Thread* self, int32_t string_length, Handle<String> string,
                                       int32_t offset, gc::AllocatorType allocator_type) {
  SetStringCountAndValueVisitorFromString visitor(string_length, string, offset);
  String* new_string = Alloc<kIsInstrumented>(self, string_length, allocator_type, visitor);
  return new_string;
}

inline int32_t String::GetHashCode() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_));
  if (UNLIKELY(result == 0)) {
    result = ComputeHashCode();
  }
  DCHECK(result != 0 || ComputeUtf16Hash(GetValue(), GetLength()) == 0)
      << ToModifiedUtf8() << " " << result;
  return result;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_INL_H_
