/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_TYPE_LOOKUP_TABLE_H_
#define ART_RUNTIME_TYPE_LOOKUP_TABLE_H_

#include "dex_file.h"
#include "leb128.h"
#include "utf.h"

namespace art {

/**
 * TypeLookupTable used to find class_def_idx by class descriptor quickly.
 * Implementation of TypeLookupTable is based on hash table.
 * This class instantiated at compile time by calling Create() method and written into OAT file.
 * At runtime, the raw data is read from memory-mapped file by calling Open() method. The table
 * memory remains clean.
 */
class TypeLookupTable {
 public:
  ~TypeLookupTable();

  // Return the number of buckets in the lookup table.
  uint32_t Size() const {
    return mask_ + 1;
  }

  // Method search class_def_idx by class descriptor and it's hash.
  // If no data found then the method returns DexFile::kDexNoIndex
  ALWAYS_INLINE uint32_t Lookup(const char* str, uint32_t hash) const {
    uint32_t pos = hash & GetSizeMask();
    // Thanks to special insertion algorithm, the element at position pos can be empty or start of
    // bucket.
    const Entry* entry = &entries_[pos];
    while (!entry->IsEmpty()) {
      if (CmpHashBits(entry->data, hash) && IsStringsEquals(str, entry->str_offset)) {
        return GetClassDefIdx(entry->data);
      }
      if (entry->IsLast()) {
        return DexFile::kDexNoIndex;
      }
      pos = (pos + entry->next_pos_delta) & GetSizeMask();
      entry = &entries_[pos];
    }
    return DexFile::kDexNoIndex;
  }

  // Method creates lookup table for dex file
  static TypeLookupTable* Create(const DexFile& dex_file, uint8_t* storage = nullptr);

  // Method opens lookup table from binary data. Lookup table does not owns binary data.
  static TypeLookupTable* Open(const uint8_t* raw_data, const DexFile& dex_file);

  // Method returns pointer to binary data of lookup table. Used by the oat writer.
  const uint8_t* RawData() const {
    return reinterpret_cast<const uint8_t*>(entries_.get());
  }

  // Method returns length of binary data. Used by the oat writer.
  uint32_t RawDataLength() const;

  // Method returns length of binary data for the specified dex file.
  static uint32_t RawDataLength(const DexFile& dex_file);

  // Method returns length of binary data for the specified number of class definitions.
  static uint32_t RawDataLength(uint32_t num_class_defs);

 private:
   /**
    * To find element we need to compare strings.
    * It is faster to compare first hashes and then strings itself.
    * But we have no full hash of element of table. But we can use 2 ideas.
    * 1. All minor bits of hash inside one bucket are equals.
    * 2. If dex file contains N classes and size of hash table is 2^n (where N <= 2^n)
    *    then 16-n bits are free. So we can encode part of element's hash into these bits.
    * So hash of element can be divided on three parts:
    * XXXX XXXX XXXX YYYY YZZZ ZZZZ ZZZZZ
    * Z - a part of hash encoded in bucket (these bits of has are same for all elements in bucket) -
    * n bits
    * Y - a part of hash that we can write into free 16-n bits (because only n bits used to store
    * class_def_idx)
    * X - a part of has that we can't use without increasing increase
    * So the data element of Entry used to store class_def_idx and part of hash of the entry.
    */
  struct Entry {
    uint32_t str_offset;
    uint16_t data;
    uint16_t next_pos_delta;

    Entry() : str_offset(0), data(0), next_pos_delta(0) {}

    bool IsEmpty() const {
      return str_offset == 0;
    }

    bool IsLast() const {
      return next_pos_delta == 0;
    }
  };

  static uint32_t CalculateMask(uint32_t num_class_defs);
  static bool SupportedSize(uint32_t num_class_defs);

  // Construct from a dex file.
  explicit TypeLookupTable(const DexFile& dex_file, uint8_t* storage);

  // Construct from a dex file with existing data.
  TypeLookupTable(const uint8_t* raw_data, const DexFile& dex_file);

  bool IsStringsEquals(const char* str, uint32_t str_offset) const {
    const uint8_t* ptr = dex_file_.Begin() + str_offset;
    // Skip string length.
    DecodeUnsignedLeb128(&ptr);
    return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(
        str, reinterpret_cast<const char*>(ptr)) == 0;
  }

  // Method extracts hash bits from element's data and compare them with
  // the corresponding bits of the specified hash
  bool CmpHashBits(uint32_t data, uint32_t hash) const {
    uint32_t mask = static_cast<uint16_t>(~GetSizeMask());
    return (hash & mask) == (data & mask);
  }

  uint32_t GetClassDefIdx(uint32_t data) const {
    return data & mask_;
  }

  uint32_t GetSizeMask() const {
    return mask_;
  }

  // Attempt to set an entry on it's hash' slot. If there is alrady something there, return false.
  // Otherwise return true.
  bool SetOnInitialPos(const Entry& entry, uint32_t hash);

  // Insert an entry, probes until there is an empty slot.
  void Insert(const Entry& entry, uint32_t hash);

  // Find the last entry in a chain.
  uint32_t FindLastEntryInBucket(uint32_t cur_pos) const;

  const DexFile& dex_file_;
  const uint32_t mask_;
  std::unique_ptr<Entry[]> entries_;
  // owns_entries_ specifies if the lookup table owns the entries_ array.
  const bool owns_entries_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TypeLookupTable);
};

}  // namespace art

#endif  // ART_RUNTIME_TYPE_LOOKUP_TABLE_H_
