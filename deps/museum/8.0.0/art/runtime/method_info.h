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

#ifndef ART_RUNTIME_METHOD_INFO_H_
#define ART_RUNTIME_METHOD_INFO_H_

#include "base/logging.h"
#include "leb128.h"
#include "memory_region.h"

namespace art {

// Method info is for not dedupe friendly data of a method. Currently it only holds methods indices.
// Putting this data in MethodInfo instead of code infos saves ~5% oat size.
class MethodInfo {
  using MethodIndexType = uint16_t;

 public:
  // Reading mode
  explicit MethodInfo(const uint8_t* ptr) {
    if (ptr != nullptr) {
      num_method_indices_ = DecodeUnsignedLeb128(&ptr);
      region_ = MemoryRegion(const_cast<uint8_t*>(ptr),
                             num_method_indices_ * sizeof(MethodIndexType));
    }
  }

  // Writing mode
  MethodInfo(uint8_t* ptr, size_t num_method_indices) : num_method_indices_(num_method_indices) {
    DCHECK(ptr != nullptr);
    ptr = EncodeUnsignedLeb128(ptr, num_method_indices_);
    region_ = MemoryRegion(ptr, num_method_indices_ * sizeof(MethodIndexType));
  }

  static size_t ComputeSize(size_t num_method_indices) {
    uint8_t temp[8];
    uint8_t* ptr = temp;
    ptr = EncodeUnsignedLeb128(ptr, num_method_indices);
    return (ptr - temp) + num_method_indices * sizeof(MethodIndexType);
  }

  ALWAYS_INLINE MethodIndexType GetMethodIndex(size_t index) const {
    // Use bit functions to avoid pesky alignment requirements.
    return region_.LoadBits(index * BitSizeOf<MethodIndexType>(), BitSizeOf<MethodIndexType>());
  }

  void SetMethodIndex(size_t index, MethodIndexType method_index) {
    region_.StoreBits(index * BitSizeOf<MethodIndexType>(),
                      method_index,
                      BitSizeOf<MethodIndexType>());
  }

  size_t NumMethodIndices() const {
    return num_method_indices_;
  }

 private:
  size_t num_method_indices_ = 0u;
  MemoryRegion region_;
};

}  // namespace art

#endif  // ART_RUNTIME_METHOD_INFO_H_
