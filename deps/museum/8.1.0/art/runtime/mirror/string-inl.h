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

#include "string.h"

#include "android-base/stringprintf.h"

#include "array.h"
#include "base/bit_utils.h"
#include "class.h"
#include "common_throws.h"
#include "gc/heap-inl.h"
#include "globals.h"
#include "runtime.h"
#include "thread.h"
#include "utf.h"
#include "utils.h"

namespace art {
namespace mirror {

inline uint32_t String::ClassSize(PointerSize pointer_size) {
  uint32_t vtable_entries = Object::kVTableLength + 56;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 1, 2, pointer_size);
}

// Sets string count in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountVisitor {
 public:
  explicit SetStringCountVisitor(int32_t count) : count_(count) {
  }

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    ObjPtr<String> string = ObjPtr<String>::DownCast(obj);
    string->SetCount(count_);
    DCHECK(!string->IsCompressed() || kUseStringCompression);
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

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    ObjPtr<String> string = ObjPtr<String>::DownCast(obj);
    string->SetCount(count_);
    DCHECK(!string->IsCompressed() || kUseStringCompression);
    int32_t length = String::GetLengthFromCount(count_);
    const uint8_t* const src = reinterpret_cast<uint8_t*>(src_array_->GetData()) + offset_;
    if (string->IsCompressed()) {
      uint8_t* valueCompressed = string->GetValueCompressed();
      for (int i = 0; i < length; i++) {
        valueCompressed[i] = (src[i] & 0xFF);
      }
    } else {
      uint16_t* value = string->GetValue();
      for (int i = 0; i < length; i++) {
        value[i] = high_byte_ + (src[i] & 0xFF);
      }
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

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    ObjPtr<String> string = ObjPtr<String>::DownCast(obj);
    string->SetCount(count_);
    const uint16_t* const src = src_array_->GetData() + offset_;
    const int32_t length = String::GetLengthFromCount(count_);
    if (kUseStringCompression && String::IsCompressed(count_)) {
      for (int i = 0; i < length; ++i) {
        string->GetValueCompressed()[i] = static_cast<uint8_t>(src[i]);
      }
    } else {
      memcpy(string->GetValue(), src, length * sizeof(uint16_t));
    }
  }

 private:
  const int32_t count_;
  Handle<CharArray> src_array_;
  const int32_t offset_;
};

// Sets string count and value in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountAndValueVisitorFromString {
 public:
  SetStringCountAndValueVisitorFromString(int32_t count,
                                          Handle<String> src_string,
                                          int32_t offset) :
    count_(count), src_string_(src_string), offset_(offset) {
  }

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    ObjPtr<String> string = ObjPtr<String>::DownCast(obj);
    string->SetCount(count_);
    const int32_t length = String::GetLengthFromCount(count_);
    bool compressible = kUseStringCompression && String::IsCompressed(count_);
    if (src_string_->IsCompressed()) {
      const uint8_t* const src = src_string_->GetValueCompressed() + offset_;
      memcpy(string->GetValueCompressed(), src, length * sizeof(uint8_t));
    } else {
      const uint16_t* const src = src_string_->GetValue() + offset_;
      if (compressible) {
        for (int i = 0; i < length; ++i) {
          string->GetValueCompressed()[i] = static_cast<uint8_t>(src[i]);
        }
      } else {
        memcpy(string->GetValue(), src, length * sizeof(uint16_t));
      }
    }
  }

 private:
  const int32_t count_;
  Handle<String> src_string_;
  const int32_t offset_;
};

inline uint16_t String::CharAt(int32_t index) {
  int32_t count = GetLength();
  if (UNLIKELY((index < 0) || (index >= count))) {
    ThrowStringIndexOutOfBoundsException(index, count);
    return 0;
  }
  if (IsCompressed()) {
    return GetValueCompressed()[index];
  } else {
    return GetValue()[index];
  }
}

template <typename MemoryType>
int32_t String::FastIndexOf(MemoryType* chars, int32_t ch, int32_t start) {
  const MemoryType* p = chars + start;
  const MemoryType* end = chars + GetLength();
  while (p < end) {
    if (*p++ == ch) {
      return (p - 1) - chars;
    }
  }
  return -1;
}

template<VerifyObjectFlags kVerifyFlags>
inline size_t String::SizeOf() {
  size_t size = sizeof(String);
  if (IsCompressed()) {
    size += (sizeof(uint8_t) * GetLength<kVerifyFlags>());
  } else {
    size += (sizeof(uint16_t) * GetLength<kVerifyFlags>());
  }
  // String.equals() intrinsics assume zero-padding up to kObjectAlignment,
  // so make sure the zero-padding is actually copied around if GC compaction
  // chooses to copy only SizeOf() bytes.
  // http://b/23528461
  return RoundUp(size, kObjectAlignment);
}

template <bool kIsInstrumented, typename PreFenceVisitor>
inline String* String::Alloc(Thread* self, int32_t utf16_length_with_flag,
                             gc::AllocatorType allocator_type,
                             const PreFenceVisitor& pre_fence_visitor) {
  constexpr size_t header_size = sizeof(String);
  const bool compressible = kUseStringCompression && String::IsCompressed(utf16_length_with_flag);
  const size_t block_size = (compressible) ? sizeof(uint8_t) : sizeof(uint16_t);
  size_t length = String::GetLengthFromCount(utf16_length_with_flag);
  static_assert(sizeof(length) <= sizeof(size_t),
                "static_cast<size_t>(utf16_length) must not lose bits.");
  size_t data_size = block_size * length;
  size_t size = header_size + data_size;
  // String.equals() intrinsics assume zero-padding up to kObjectAlignment,
  // so make sure the allocator clears the padding as well.
  // http://b/23528461
  size_t alloc_size = RoundUp(size, kObjectAlignment);

  Class* string_class = GetJavaLangString();
  // Check for overflow and throw OutOfMemoryError if this was an unreasonable request.
  // Do this by comparing with the maximum length that will _not_ cause an overflow.
  const size_t overflow_length = (-header_size) / block_size;   // Unsigned arithmetic.
  const size_t max_alloc_length = overflow_length - 1u;
  static_assert(IsAligned<sizeof(uint16_t)>(kObjectAlignment),
                "kObjectAlignment must be at least as big as Java char alignment");
  const size_t max_length = RoundDown(max_alloc_length, kObjectAlignment / block_size);
  if (UNLIKELY(length > max_length)) {
    self->ThrowOutOfMemoryError(
        android::base::StringPrintf("%s of length %d would overflow",
                                    Class::PrettyDescriptor(string_class).c_str(),
                                    static_cast<int>(length)).c_str());
    return nullptr;
  }

  gc::Heap* heap = Runtime::Current()->GetHeap();
  return down_cast<String*>(
      heap->AllocObjectWithAllocator<kIsInstrumented, true>(self, string_class, alloc_size,
                                                            allocator_type, pre_fence_visitor));
}

template <bool kIsInstrumented>
inline String* String::AllocEmptyString(Thread* self, gc::AllocatorType allocator_type) {
  const int32_t length_with_flag = String::GetFlaggedCount(0, /* compressible */ true);
  SetStringCountVisitor visitor(length_with_flag);
  return Alloc<kIsInstrumented>(self, length_with_flag, allocator_type, visitor);
}

template <bool kIsInstrumented>
inline String* String::AllocFromByteArray(Thread* self, int32_t byte_length,
                                          Handle<ByteArray> array, int32_t offset,
                                          int32_t high_byte, gc::AllocatorType allocator_type) {
  const uint8_t* const src = reinterpret_cast<uint8_t*>(array->GetData()) + offset;
  high_byte &= 0xff;  // Extract the relevant bits before determining `compressible`.
  const bool compressible =
      kUseStringCompression && String::AllASCII<uint8_t>(src, byte_length) && (high_byte == 0);
  const int32_t length_with_flag = String::GetFlaggedCount(byte_length, compressible);
  SetStringCountAndBytesVisitor visitor(length_with_flag, array, offset, high_byte << 8);
  String* string = Alloc<kIsInstrumented>(self, length_with_flag, allocator_type, visitor);
  return string;
}

template <bool kIsInstrumented>
inline String* String::AllocFromCharArray(Thread* self, int32_t count,
                                          Handle<CharArray> array, int32_t offset,
                                          gc::AllocatorType allocator_type) {
  // It is a caller error to have a count less than the actual array's size.
  DCHECK_GE(array->GetLength(), count);
  const bool compressible = kUseStringCompression &&
                            String::AllASCII<uint16_t>(array->GetData() + offset, count);
  const int32_t length_with_flag = String::GetFlaggedCount(count, compressible);
  SetStringCountAndValueVisitorFromCharArray visitor(length_with_flag, array, offset);
  String* new_string = Alloc<kIsInstrumented>(self, length_with_flag, allocator_type, visitor);
  return new_string;
}

template <bool kIsInstrumented>
inline String* String::AllocFromString(Thread* self, int32_t string_length, Handle<String> string,
                                       int32_t offset, gc::AllocatorType allocator_type) {
  const bool compressible = kUseStringCompression &&
      ((string->IsCompressed()) ? true : String::AllASCII<uint16_t>(string->GetValue() + offset,
                                                                    string_length));
  const int32_t length_with_flag = String::GetFlaggedCount(string_length, compressible);
  SetStringCountAndValueVisitorFromString visitor(length_with_flag, string, offset);
  String* new_string = Alloc<kIsInstrumented>(self, length_with_flag, allocator_type, visitor);
  return new_string;
}

inline int32_t String::GetHashCode() {
  int32_t result = GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_));
  if (UNLIKELY(result == 0)) {
    result = ComputeHashCode();
  }
  if (kIsDebugBuild) {
    if (IsCompressed()) {
      DCHECK(result != 0 || ComputeUtf16Hash(GetValueCompressed(), GetLength()) == 0)
          << ToModifiedUtf8() << " " << result;
    } else {
      DCHECK(result != 0 || ComputeUtf16Hash(GetValue(), GetLength()) == 0)
          << ToModifiedUtf8() << " " << result;
    }
  }
  return result;
}

template<typename MemoryType>
inline bool String::AllASCII(const MemoryType* chars, const int length) {
  static_assert(std::is_unsigned<MemoryType>::value, "Expecting unsigned MemoryType");
  for (int i = 0; i < length; ++i) {
    if (!IsASCII(chars[i])) {
      return false;
    }
  }
  return true;
}

inline bool String::DexFileStringAllASCII(const char* chars, const int length) {
  // For strings from the dex file we just need to check that
  // the terminating character is at the right position.
  DCHECK_EQ(AllASCII(reinterpret_cast<const uint8_t*>(chars), length), chars[length] == 0);
  return chars[length] == 0;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_INL_H_
