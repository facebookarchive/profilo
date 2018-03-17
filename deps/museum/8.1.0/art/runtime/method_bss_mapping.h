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

#ifndef ART_RUNTIME_METHOD_BSS_MAPPING_H_
#define ART_RUNTIME_METHOD_BSS_MAPPING_H_

#include "base/bit_utils.h"
#include "base/length_prefixed_array.h"

namespace art {

// MethodBssMappingEntry describes a mapping of up to 17 method indexes to their offsets
// in the .bss. The highest index and its associated .bss offset are stored in plain form
// as `method_index` and `bss_offset`, respectively, while the additional indexes can be
// stored in compressed form if their associated .bss entries are consecutive and in the
// method index order. Each of the 16 bits of the `index_mask` corresponds to one of the
// previous 16 method indexes and indicates whether there is a .bss entry for that index.
//
struct MethodBssMappingEntry {
  bool CoversIndex(uint32_t method_idx) const {
    uint32_t diff = method_index - method_idx;
    return (diff == 0) || (diff <= 16 && ((index_mask >> (16u - diff)) & 1u) != 0);
  }

  uint32_t GetBssOffset(uint32_t method_idx, size_t entry_size) const {
    DCHECK(CoversIndex(method_idx));
    uint32_t diff = method_index - method_idx;
    if (diff == 0) {
      return bss_offset;
    } else {
      return bss_offset - POPCOUNT(index_mask >> (16u - diff)) * entry_size;
    }
  }

  uint16_t method_index;
  uint16_t index_mask;
  uint32_t bss_offset;
};

using MethodBssMapping = LengthPrefixedArray<MethodBssMappingEntry>;

}  // namespace art

#endif  // ART_RUNTIME_METHOD_BSS_MAPPING_H_
