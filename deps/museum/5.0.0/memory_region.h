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

#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {

// Memory regions are useful for accessing memory with bounds check in
// debug mode. They can be safely passed by value and do not assume ownership
// of the region.
class MemoryRegion {
 public:
  MemoryRegion() : pointer_(NULL), size_(0) {}
  MemoryRegion(void* pointer, uword size) : pointer_(pointer), size_(size) {}

  void* pointer() const { return pointer_; }
  size_t size() const { return size_; }
  size_t size_in_bits() const { return size_ * kBitsPerByte; }

  static size_t pointer_offset() {
    return OFFSETOF_MEMBER(MemoryRegion, pointer_);
  }

  byte* start() const { return reinterpret_cast<byte*>(pointer_); }
  byte* end() const { return start() + size_; }

  template<typename T> T Load(uintptr_t offset) const {
    return *ComputeInternalPointer<T>(offset);
  }

  template<typename T> void Store(uintptr_t offset, T value) const {
    *ComputeInternalPointer<T>(offset) = value;
  }

  template<typename T> T* PointerTo(uintptr_t offset) const {
    return ComputeInternalPointer<T>(offset);
  }

  // Load a single bit in the region. The bit at offset 0 is the least
  // significant bit in the first byte.
  bool LoadBit(uintptr_t bit_offset) const {
    uint8_t bit_mask;
    uint8_t byte = *ComputeBitPointer(bit_offset, &bit_mask);
    return byte & bit_mask;
  }

  void StoreBit(uintptr_t bit_offset, bool value) const {
    uint8_t bit_mask;
    uint8_t* byte = ComputeBitPointer(bit_offset, &bit_mask);
    if (value) {
      *byte |= bit_mask;
    } else {
      *byte &= ~bit_mask;
    }
  }

  void CopyFrom(size_t offset, const MemoryRegion& from) const;

  // Compute a sub memory region based on an existing one.
  MemoryRegion Subregion(uintptr_t offset, uintptr_t size) const {
    CHECK_GE(this->size(), size);
    CHECK_LE(offset,  this->size() - size);
    return MemoryRegion(reinterpret_cast<void*>(start() + offset), size);
  }

  // Compute an extended memory region based on an existing one.
  void Extend(const MemoryRegion& region, uintptr_t extra) {
    pointer_ = region.pointer();
    size_ = (region.size() + extra);
  }

 private:
  template<typename T> T* ComputeInternalPointer(size_t offset) const {
    CHECK_GE(size(), sizeof(T));
    CHECK_LE(offset, size() - sizeof(T));
    return reinterpret_cast<T*>(start() + offset);
  }

  // Locate the bit with the given offset. Returns a pointer to the byte
  // containing the bit, and sets bit_mask to the bit within that byte.
  byte* ComputeBitPointer(uintptr_t bit_offset, byte* bit_mask) const {
    uintptr_t bit_remainder = (bit_offset & (kBitsPerByte - 1));
    *bit_mask = (1U << bit_remainder);
    uintptr_t byte_offset = (bit_offset >> kBitsPerByteLog2);
    return ComputeInternalPointer<byte>(byte_offset);
  }

  void* pointer_;
  size_t size_;
};

}  // namespace art

#endif  // ART_RUNTIME_MEMORY_REGION_H_
