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

#ifndef ART_RUNTIME_MEMORY_REGION_H_
#define ART_RUNTIME_MEMORY_REGION_H_

#include <stdint.h>
#include <type_traits>

#include "arch/instruction_set.h"
#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/value_object.h"
#include "globals.h"

namespace art {

// Memory regions are useful for accessing memory with bounds check in
// debug mode. They can be safely passed by value and do not assume ownership
// of the region.
class MemoryRegion FINAL : public ValueObject {
 public:
  MemoryRegion() : pointer_(nullptr), size_(0) {}
  MemoryRegion(void* pointer_in, uintptr_t size_in) : pointer_(pointer_in), size_(size_in) {}

  void* pointer() const { return pointer_; }
  size_t size() const { return size_; }
  size_t size_in_bits() const { return size_ * kBitsPerByte; }

  static size_t pointer_offset() {
    return OFFSETOF_MEMBER(MemoryRegion, pointer_);
  }

  uint8_t* start() const { return reinterpret_cast<uint8_t*>(pointer_); }
  uint8_t* end() const { return start() + size_; }

  // Load value of type `T` at `offset`.  The memory address corresponding
  // to `offset` should be word-aligned (on ARM, this is a requirement).
  template<typename T>
  ALWAYS_INLINE T Load(uintptr_t offset) const {
    T* address = ComputeInternalPointer<T>(offset);
    DCHECK(IsWordAligned(address));
    return *address;
  }

  // Store `value` (of type `T`) at `offset`.  The memory address
  // corresponding to `offset` should be word-aligned (on ARM, this is
  // a requirement).
  template<typename T>
  ALWAYS_INLINE void Store(uintptr_t offset, T value) const {
    T* address = ComputeInternalPointer<T>(offset);
    DCHECK(IsWordAligned(address));
    *address = value;
  }

  // Load value of type `T` at `offset`.  The memory address corresponding
  // to `offset` does not need to be word-aligned.
  template<typename T>
  ALWAYS_INLINE T LoadUnaligned(uintptr_t offset) const {
    // Equivalent unsigned integer type corresponding to T.
    typedef typename std::make_unsigned<T>::type U;
    U equivalent_unsigned_integer_value = 0;
    // Read the value byte by byte in a little-endian fashion.
    for (size_t i = 0; i < sizeof(U); ++i) {
      equivalent_unsigned_integer_value +=
          *ComputeInternalPointer<uint8_t>(offset + i) << (i * kBitsPerByte);
    }
    return bit_cast<T, U>(equivalent_unsigned_integer_value);
  }

  // Store `value` (of type `T`) at `offset`.  The memory address
  // corresponding to `offset` does not need to be word-aligned.
  template<typename T>
  ALWAYS_INLINE void StoreUnaligned(uintptr_t offset, T value) const {
    // Equivalent unsigned integer type corresponding to T.
    typedef typename std::make_unsigned<T>::type U;
    U equivalent_unsigned_integer_value = bit_cast<U, T>(value);
    // Write the value byte by byte in a little-endian fashion.
    for (size_t i = 0; i < sizeof(U); ++i) {
      *ComputeInternalPointer<uint8_t>(offset + i) =
          (equivalent_unsigned_integer_value >> (i * kBitsPerByte)) & 0xFF;
    }
  }

  template<typename T>
  ALWAYS_INLINE T* PointerTo(uintptr_t offset) const {
    return ComputeInternalPointer<T>(offset);
  }

  // Load a single bit in the region. The bit at offset 0 is the least
  // significant bit in the first byte.
  ALWAYS_INLINE bool LoadBit(uintptr_t bit_offset) const {
    uint8_t bit_mask;
    uint8_t byte = *ComputeBitPointer(bit_offset, &bit_mask);
    return byte & bit_mask;
  }

  ALWAYS_INLINE void StoreBit(uintptr_t bit_offset, bool value) const {
    uint8_t bit_mask;
    uint8_t* byte = ComputeBitPointer(bit_offset, &bit_mask);
    if (value) {
      *byte |= bit_mask;
    } else {
      *byte &= ~bit_mask;
    }
  }

  // Load `length` bits from the region starting at bit offset `bit_offset`.
  // The bit at the smallest offset is the least significant bit in the
  // loaded value.  `length` must not be larger than the number of bits
  // contained in the return value (32).
  uint32_t LoadBits(uintptr_t bit_offset, size_t length) const {
    CHECK_LE(length, sizeof(uint32_t) * kBitsPerByte);
    uint32_t value = 0u;
    for (size_t i = 0; i < length; ++i) {
      value |= LoadBit(bit_offset + i) << i;
    }
    return value;
  }

  // Store `value` on `length` bits in the region starting at bit offset
  // `bit_offset`.  The bit at the smallest offset is the least significant
  // bit of the stored `value`.  `value` must not be larger than `length`
  // bits.
  void StoreBits(uintptr_t bit_offset, uint32_t value, size_t length) {
    CHECK_LT(value, 2u << length);
    for (size_t i = 0; i < length; ++i) {
      bool ith_bit = value & (1 << i);
      StoreBit(bit_offset + i, ith_bit);
    }
  }

  void CopyFrom(size_t offset, const MemoryRegion& from) const;

  // Compute a sub memory region based on an existing one.
  MemoryRegion Subregion(uintptr_t offset, uintptr_t size_in) const {
    CHECK_GE(this->size(), size_in);
    CHECK_LE(offset,  this->size() - size_in);
    return MemoryRegion(reinterpret_cast<void*>(start() + offset), size_in);
  }

  // Compute an extended memory region based on an existing one.
  void Extend(const MemoryRegion& region, uintptr_t extra) {
    pointer_ = region.pointer();
    size_ = (region.size() + extra);
  }

 private:
  template<typename T>
  ALWAYS_INLINE T* ComputeInternalPointer(size_t offset) const {
    CHECK_GE(size(), sizeof(T));
    CHECK_LE(offset, size() - sizeof(T));
    return reinterpret_cast<T*>(start() + offset);
  }

  // Locate the bit with the given offset. Returns a pointer to the byte
  // containing the bit, and sets bit_mask to the bit within that byte.
  ALWAYS_INLINE uint8_t* ComputeBitPointer(uintptr_t bit_offset, uint8_t* bit_mask) const {
    uintptr_t bit_remainder = (bit_offset & (kBitsPerByte - 1));
    *bit_mask = (1U << bit_remainder);
    uintptr_t byte_offset = (bit_offset >> kBitsPerByteLog2);
    return ComputeInternalPointer<uint8_t>(byte_offset);
  }

  // Is `address` aligned on a machine word?
  template<typename T> static bool IsWordAligned(const T* address) {
    // Word alignment in bytes.
    size_t kWordAlignment = GetInstructionSetPointerSize(kRuntimeISA);
    return IsAlignedParam(address, kWordAlignment);
  }

  void* pointer_;
  size_t size_;
};

}  // namespace art

#endif  // ART_RUNTIME_MEMORY_REGION_H_
