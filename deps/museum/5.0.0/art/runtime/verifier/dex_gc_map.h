/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_DEX_GC_MAP_H_
#define ART_RUNTIME_VERIFIER_DEX_GC_MAP_H_

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"

namespace art {
namespace verifier {

/*
 * Format enumeration for RegisterMap data area.
 */
enum RegisterMapFormat {
  kRegMapFormatUnknown = 0,
  kRegMapFormatNone = 1,       // Indicates no map data follows.
  kRegMapFormatCompact8 = 2,   // Compact layout, 8-bit addresses.
  kRegMapFormatCompact16 = 3,  // Compact layout, 16-bit addresses.
};

// Lightweight wrapper for Dex PC to reference bit maps.
class DexPcToReferenceMap {
 public:
  explicit DexPcToReferenceMap(const uint8_t* data) : data_(data) {
    CHECK(data_ != NULL);
  }

  // The total size of the reference bit map including header.
  size_t RawSize() const {
    return EntryWidth() * NumEntries() + 4u /* header */;
  }

  // The number of entries in the table
  size_t NumEntries() const {
    return GetData()[2] | (GetData()[3] << 8);
  }

  // Get the Dex PC at the given index
  uint16_t GetDexPc(size_t index) const {
    size_t entry_offset = index * EntryWidth();
    if (DexPcWidth() == 1) {
      return Table()[entry_offset];
    } else {
      return Table()[entry_offset] | (Table()[entry_offset + 1] << 8);
    }
  }

  // Return address of bitmap encoding what are live references
  const uint8_t* GetBitMap(size_t index) const {
    size_t entry_offset = index * EntryWidth();
    return &Table()[entry_offset + DexPcWidth()];
  }

  // Find the bitmap associated with the given dex pc
  const uint8_t* FindBitMap(uint16_t dex_pc, bool error_if_not_present = true) const;

  // The number of bytes used to encode registers
  size_t RegWidth() const {
    return GetData()[1] | ((GetData()[0] & ~kRegMapFormatMask) << kRegMapFormatShift);
  }

 private:
  // Table of num_entries * (dex pc, bitmap)
  const uint8_t* Table() const {
    return GetData() + 4;
  }

  // The format of the table of the PCs for the table
  RegisterMapFormat Format() const {
    return static_cast<RegisterMapFormat>(GetData()[0] & kRegMapFormatMask);
  }

  // Number of bytes used to encode a dex pc
  size_t DexPcWidth() const {
    RegisterMapFormat format = Format();
    switch (format) {
      case kRegMapFormatCompact8:
        return 1;
      case kRegMapFormatCompact16:
        return 2;
      default:
        LOG(FATAL) << "Invalid format " << static_cast<int>(format);
        return -1;
    }
  }

  // The width of an entry in the table
  size_t EntryWidth() const {
    return DexPcWidth() + RegWidth();
  }

  const uint8_t* GetData() const {
    return data_;
  }

  static const int kRegMapFormatShift = 5;
  static const uint8_t kRegMapFormatMask = 0x7;

  const uint8_t* const data_;  // The header and table data
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_DEX_GC_MAP_H_
