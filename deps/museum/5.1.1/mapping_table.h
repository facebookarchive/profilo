/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_MAPPING_TABLE_H_
#define ART_RUNTIME_MAPPING_TABLE_H_

#include "base/logging.h"
#include "leb128.h"

namespace art {

// A utility for processing the raw uleb128 encoded mapping table created by the quick compiler.
class MappingTable {
 public:
  explicit MappingTable(const uint8_t* encoded_map) : encoded_table_(encoded_map) {
  }

  uint32_t TotalSize() const PURE {
    const uint8_t* table = encoded_table_;
    if (table == nullptr) {
      return 0;
    } else {
      return DecodeUnsignedLeb128(&table);
    }
  }

  uint32_t DexToPcSize() const PURE {
    const uint8_t* table = encoded_table_;
    if (table == nullptr) {
      return 0;
    } else {
      uint32_t total_size = DecodeUnsignedLeb128(&table);
      uint32_t pc_to_dex_size = DecodeUnsignedLeb128(&table);
      return total_size - pc_to_dex_size;
    }
  }

  const uint8_t* FirstDexToPcPtr() const {
    const uint8_t* table = encoded_table_;
    if (table != nullptr) {
      uint32_t total_size = DecodeUnsignedLeb128(&table);
      uint32_t pc_to_dex_size = DecodeUnsignedLeb128(&table);
      // We must have dex to pc entries or else the loop will go beyond the end of the table.
      DCHECK_GT(total_size, pc_to_dex_size);
      for (uint32_t i = 0; i < pc_to_dex_size; ++i) {
        DecodeUnsignedLeb128(&table);  // Move ptr past native PC delta.
        DecodeSignedLeb128(&table);  // Move ptr past dex PC delta.
      }
    }
    return table;
  }

  class DexToPcIterator {
   public:
    DexToPcIterator(const MappingTable* table, uint32_t element) :
        table_(table), element_(element), end_(table_->DexToPcSize()), encoded_table_ptr_(nullptr),
        native_pc_offset_(0), dex_pc_(0) {
      if (element == 0) {  // An iterator wanted from the start.
        if (end_ > 0) {
          encoded_table_ptr_ = table_->FirstDexToPcPtr();
          native_pc_offset_ = DecodeUnsignedLeb128(&encoded_table_ptr_);
          // First delta is always positive.
          dex_pc_ = static_cast<uint32_t>(DecodeSignedLeb128(&encoded_table_ptr_));
        }
      } else {  // An iterator wanted from the end.
        DCHECK_EQ(table_->DexToPcSize(), element);
      }
    }
    uint32_t NativePcOffset() const {
      return native_pc_offset_;
    }
    uint32_t DexPc() const {
      return dex_pc_;
    }
    void operator++() {
      ++element_;
      if (element_ != end_) {  // Avoid reading beyond the end of the table.
        native_pc_offset_ += DecodeUnsignedLeb128(&encoded_table_ptr_);
        // For negative delta, unsigned overflow after static_cast does exactly what we need.
        dex_pc_ += static_cast<uint32_t>(DecodeSignedLeb128(&encoded_table_ptr_));
      }
    }
    bool operator==(const DexToPcIterator& rhs) const {
      CHECK(table_ == rhs.table_);
      return element_ == rhs.element_;
    }
    bool operator!=(const DexToPcIterator& rhs) const {
      CHECK(table_ == rhs.table_);
      return element_ != rhs.element_;
    }

   private:
    const MappingTable* const table_;  // The original table.
    uint32_t element_;  // A value in the range 0 to end_.
    const uint32_t end_;  // Equal to table_->DexToPcSize().
    const uint8_t* encoded_table_ptr_;  // Either nullptr or points to encoded data after this entry.
    uint32_t native_pc_offset_;  // The current value of native pc offset.
    uint32_t dex_pc_;  // The current value of dex pc.
  };

  DexToPcIterator DexToPcBegin() const {
    return DexToPcIterator(this, 0);
  }

  DexToPcIterator DexToPcEnd() const {
    uint32_t size = DexToPcSize();
    return DexToPcIterator(this, size);
  }

  uint32_t PcToDexSize() const PURE {
    const uint8_t* table = encoded_table_;
    if (table == nullptr) {
      return 0;
    } else {
      DecodeUnsignedLeb128(&table);  // Total_size, unused.
      uint32_t pc_to_dex_size = DecodeUnsignedLeb128(&table);
      return pc_to_dex_size;
    }
  }

  const uint8_t* FirstPcToDexPtr() const {
    const uint8_t* table = encoded_table_;
    if (table != nullptr) {
      DecodeUnsignedLeb128(&table);  // Total_size, unused.
      DecodeUnsignedLeb128(&table);  // PC to Dex size, unused.
    }
    return table;
  }

  class PcToDexIterator {
   public:
    PcToDexIterator(const MappingTable* table, uint32_t element) :
        table_(table), element_(element), end_(table_->PcToDexSize()), encoded_table_ptr_(nullptr),
        native_pc_offset_(0), dex_pc_(0) {
      if (element == 0) {  // An iterator wanted from the start.
        if (end_ > 0) {
          encoded_table_ptr_ = table_->FirstPcToDexPtr();
          native_pc_offset_ = DecodeUnsignedLeb128(&encoded_table_ptr_);
          // First delta is always positive.
          dex_pc_ = static_cast<uint32_t>(DecodeSignedLeb128(&encoded_table_ptr_));
        }
      } else {  // An iterator wanted from the end.
        DCHECK_EQ(table_->PcToDexSize(), element);
      }
    }
    uint32_t NativePcOffset() const {
      return native_pc_offset_;
    }
    uint32_t DexPc() const {
      return dex_pc_;
    }
    void operator++() {
      ++element_;
      if (element_ != end_) {  // Avoid reading beyond the end of the table.
        native_pc_offset_ += DecodeUnsignedLeb128(&encoded_table_ptr_);
        // For negative delta, unsigned overflow after static_cast does exactly what we need.
        dex_pc_ += static_cast<uint32_t>(DecodeSignedLeb128(&encoded_table_ptr_));
      }
    }
    bool operator==(const PcToDexIterator& rhs) const {
      CHECK(table_ == rhs.table_);
      return element_ == rhs.element_;
    }
    bool operator!=(const PcToDexIterator& rhs) const {
      CHECK(table_ == rhs.table_);
      return element_ != rhs.element_;
    }

   private:
    const MappingTable* const table_;  // The original table.
    uint32_t element_;  // A value in the range 0 to PcToDexSize.
    const uint32_t end_;  // Equal to table_->PcToDexSize().
    const uint8_t* encoded_table_ptr_;  // Either null or points to encoded data after this entry.
    uint32_t native_pc_offset_;  // The current value of native pc offset.
    uint32_t dex_pc_;  // The current value of dex pc.
  };

  PcToDexIterator PcToDexBegin() const {
    return PcToDexIterator(this, 0);
  }

  PcToDexIterator PcToDexEnd() const {
    uint32_t size = PcToDexSize();
    return PcToDexIterator(this, size);
  }

 private:
  const uint8_t* const encoded_table_;
};

}  // namespace art

#endif  // ART_RUNTIME_MAPPING_TABLE_H_
