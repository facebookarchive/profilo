/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_BIT_MEMORY_REGION_H_
#define ART_RUNTIME_BIT_MEMORY_REGION_H_

#include "memory_region.h"

namespace art {

// Bit memory region is a bit offset subregion of a normal memoryregion. This is useful for
// abstracting away the bit start offset to avoid needing passing as an argument everywhere.
class BitMemoryRegion FINAL : public ValueObject {
 public:
  BitMemoryRegion() = default;
  ALWAYS_INLINE BitMemoryRegion(MemoryRegion region, size_t bit_offset, size_t bit_size) {
    bit_start_ = bit_offset % kBitsPerByte;
    const size_t start = bit_offset / kBitsPerByte;
    const size_t end = (bit_offset + bit_size + kBitsPerByte - 1) / kBitsPerByte;
    region_ = region.Subregion(start, end - start);
  }

  void* pointer() const { return region_.pointer(); }
  size_t size() const { return region_.size(); }
  size_t BitOffset() const { return bit_start_; }
  size_t size_in_bits() const {
    return region_.size_in_bits();
  }

  ALWAYS_INLINE BitMemoryRegion Subregion(size_t bit_offset, size_t bit_size) const {
    return BitMemoryRegion(region_, bit_start_ + bit_offset, bit_size);
  }

  // Load a single bit in the region. The bit at offset 0 is the least
  // significant bit in the first byte.
  ALWAYS_INLINE bool LoadBit(uintptr_t bit_offset) const {
    return region_.LoadBit(bit_offset + bit_start_);
  }

  ALWAYS_INLINE void StoreBit(uintptr_t bit_offset, bool value) const {
    region_.StoreBit(bit_offset + bit_start_, value);
  }

  ALWAYS_INLINE uint32_t LoadBits(uintptr_t bit_offset, size_t length) const {
    return region_.LoadBits(bit_offset + bit_start_, length);
  }

  // Store at a bit offset from inside the bit memory region.
  ALWAYS_INLINE void StoreBits(uintptr_t bit_offset, uint32_t value, size_t length) {
    region_.StoreBits(bit_offset + bit_start_, value, length);
  }

 private:
  MemoryRegion region_;
  size_t bit_start_ = 0;
};

}  // namespace art

#endif  // ART_RUNTIME_BIT_MEMORY_REGION_H_
