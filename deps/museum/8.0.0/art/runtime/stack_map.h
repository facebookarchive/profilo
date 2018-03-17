/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_STACK_MAP_H_
#define ART_RUNTIME_STACK_MAP_H_

#include "arch/code_offset.h"
#include "base/bit_vector.h"
#include "base/bit_utils.h"
#include "bit_memory_region.h"
#include "dex_file.h"
#include "memory_region.h"
#include "method_info.h"
#include "leb128.h"

namespace art {

class VariableIndentationOutputStream;

// Size of a frame slot, in bytes.  This constant is a signed value,
// to please the compiler in arithmetic operations involving int32_t
// (signed) values.
static constexpr ssize_t kFrameSlotSize = 4;

// Size of Dex virtual registers.
static constexpr size_t kVRegSize = 4;

class ArtMethod;
class CodeInfo;
class StackMapEncoding;
struct CodeInfoEncoding;

/**
 * Classes in the following file are wrapper on stack map information backed
 * by a MemoryRegion. As such they read and write to the region, they don't have
 * their own fields.
 */

// Dex register location container used by DexRegisterMap and StackMapStream.
class DexRegisterLocation {
 public:
  /*
   * The location kind used to populate the Dex register information in a
   * StackMapStream can either be:
   * - kStack: vreg stored on the stack, value holds the stack offset;
   * - kInRegister: vreg stored in low 32 bits of a core physical register,
   *                value holds the register number;
   * - kInRegisterHigh: vreg stored in high 32 bits of a core physical register,
   *                    value holds the register number;
   * - kInFpuRegister: vreg stored in low 32 bits of an FPU register,
   *                   value holds the register number;
   * - kInFpuRegisterHigh: vreg stored in high 32 bits of an FPU register,
   *                       value holds the register number;
   * - kConstant: value holds the constant;
   *
   * In addition, DexRegisterMap also uses these values:
   * - kInStackLargeOffset: value holds a "large" stack offset (greater than
   *   or equal to 128 bytes);
   * - kConstantLargeValue: value holds a "large" constant (lower than 0, or
   *   or greater than or equal to 32);
   * - kNone: the register has no location, meaning it has not been set.
   */
  enum class Kind : uint8_t {
    // Short location kinds, for entries fitting on one byte (3 bits
    // for the kind, 5 bits for the value) in a DexRegisterMap.
    kInStack = 0,             // 0b000
    kInRegister = 1,          // 0b001
    kInRegisterHigh = 2,      // 0b010
    kInFpuRegister = 3,       // 0b011
    kInFpuRegisterHigh = 4,   // 0b100
    kConstant = 5,            // 0b101

    // Large location kinds, requiring a 5-byte encoding (1 byte for the
    // kind, 4 bytes for the value).

    // Stack location at a large offset, meaning that the offset value
    // divided by the stack frame slot size (4 bytes) cannot fit on a
    // 5-bit unsigned integer (i.e., this offset value is greater than
    // or equal to 2^5 * 4 = 128 bytes).
    kInStackLargeOffset = 6,  // 0b110

    // Large constant, that cannot fit on a 5-bit signed integer (i.e.,
    // lower than 0, or greater than or equal to 2^5 = 32).
    kConstantLargeValue = 7,  // 0b111

    // Entries with no location are not stored and do not need own marker.
    kNone = static_cast<uint8_t>(-1),

    kLastLocationKind = kConstantLargeValue
  };

  static_assert(
      sizeof(Kind) == 1u,
      "art::DexRegisterLocation::Kind has a size different from one byte.");

  static bool IsShortLocationKind(Kind kind) {
    switch (kind) {
      case Kind::kInStack:
      case Kind::kInRegister:
      case Kind::kInRegisterHigh:
      case Kind::kInFpuRegister:
      case Kind::kInFpuRegisterHigh:
      case Kind::kConstant:
        return true;

      case Kind::kInStackLargeOffset:
      case Kind::kConstantLargeValue:
        return false;

      case Kind::kNone:
        LOG(FATAL) << "Unexpected location kind";
    }
    UNREACHABLE();
  }

  // Convert `kind` to a "surface" kind, i.e. one that doesn't include
  // any value with a "large" qualifier.
  // TODO: Introduce another enum type for the surface kind?
  static Kind ConvertToSurfaceKind(Kind kind) {
    switch (kind) {
      case Kind::kInStack:
      case Kind::kInRegister:
      case Kind::kInRegisterHigh:
      case Kind::kInFpuRegister:
      case Kind::kInFpuRegisterHigh:
      case Kind::kConstant:
        return kind;

      case Kind::kInStackLargeOffset:
        return Kind::kInStack;

      case Kind::kConstantLargeValue:
        return Kind::kConstant;

      case Kind::kNone:
        return kind;
    }
    UNREACHABLE();
  }

  // Required by art::StackMapStream::LocationCatalogEntriesIndices.
  DexRegisterLocation() : kind_(Kind::kNone), value_(0) {}

  DexRegisterLocation(Kind kind, int32_t value) : kind_(kind), value_(value) {}

  static DexRegisterLocation None() {
    return DexRegisterLocation(Kind::kNone, 0);
  }

  // Get the "surface" kind of the location, i.e., the one that doesn't
  // include any value with a "large" qualifier.
  Kind GetKind() const {
    return ConvertToSurfaceKind(kind_);
  }

  // Get the value of the location.
  int32_t GetValue() const { return value_; }

  // Get the actual kind of the location.
  Kind GetInternalKind() const { return kind_; }

  bool operator==(DexRegisterLocation other) const {
    return kind_ == other.kind_ && value_ == other.value_;
  }

  bool operator!=(DexRegisterLocation other) const {
    return !(*this == other);
  }

 private:
  Kind kind_;
  int32_t value_;

  friend class DexRegisterLocationHashFn;
};

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation::Kind& kind);

/**
 * Store information on unique Dex register locations used in a method.
 * The information is of the form:
 *
 *   [DexRegisterLocation+].
 *
 * DexRegisterLocations are either 1- or 5-byte wide (see art::DexRegisterLocation::Kind).
 */
class DexRegisterLocationCatalog {
 public:
  explicit DexRegisterLocationCatalog(MemoryRegion region) : region_(region) {}

  // Short (compressed) location, fitting on one byte.
  typedef uint8_t ShortLocation;

  void SetRegisterInfo(size_t offset, const DexRegisterLocation& dex_register_location) {
    DexRegisterLocation::Kind kind = ComputeCompressedKind(dex_register_location);
    int32_t value = dex_register_location.GetValue();
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Compress the kind and the value as a single byte.
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Instead of storing stack offsets expressed in bytes for
        // short stack locations, store slot offsets.  A stack offset
        // is a multiple of 4 (kFrameSlotSize).  This means that by
        // dividing it by 4, we can fit values from the [0, 128)
        // interval in a short stack location, and not just values
        // from the [0, 32) interval.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      DCHECK(IsShortValue(value)) << value;
      region_.StoreUnaligned<ShortLocation>(offset, MakeShortLocation(kind, value));
    } else {
      // Large location.  Write the location on one byte and the value
      // on 4 bytes.
      DCHECK(!IsShortValue(value)) << value;
      if (kind == DexRegisterLocation::Kind::kInStackLargeOffset) {
        // Also divide large stack offsets by 4 for the sake of consistency.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      // Data can be unaligned as the written Dex register locations can
      // either be 1-byte or 5-byte wide.  Use
      // art::MemoryRegion::StoreUnaligned instead of
      // art::MemoryRegion::Store to prevent unligned word accesses on ARM.
      region_.StoreUnaligned<DexRegisterLocation::Kind>(offset, kind);
      region_.StoreUnaligned<int32_t>(offset + sizeof(DexRegisterLocation::Kind), value);
    }
  }

  // Find the offset of the location catalog entry number `location_catalog_entry_index`.
  size_t FindLocationOffset(size_t location_catalog_entry_index) const {
    size_t offset = kFixedSize;
    // Skip the first `location_catalog_entry_index - 1` entries.
    for (uint16_t i = 0; i < location_catalog_entry_index; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a large location.
      DexRegisterLocation::Kind kind = ExtractKindAtOffset(offset);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += SingleShortEntrySize();
      } else {
        // Large location.  Skip the 5 next bytes.
        offset += SingleLargeEntrySize();
      }
    }
    return offset;
  }

  // Get the internal kind of entry at `location_catalog_entry_index`.
  DexRegisterLocation::Kind GetLocationInternalKind(size_t location_catalog_entry_index) const {
    if (location_catalog_entry_index == kNoLocationEntryIndex) {
      return DexRegisterLocation::Kind::kNone;
    }
    return ExtractKindAtOffset(FindLocationOffset(location_catalog_entry_index));
  }

  // Get the (surface) kind and value of entry at `location_catalog_entry_index`.
  DexRegisterLocation GetDexRegisterLocation(size_t location_catalog_entry_index) const {
    if (location_catalog_entry_index == kNoLocationEntryIndex) {
      return DexRegisterLocation::None();
    }
    size_t offset = FindLocationOffset(location_catalog_entry_index);
    // Read the first byte and inspect its first 3 bits to get the location.
    ShortLocation first_byte = region_.LoadUnaligned<ShortLocation>(offset);
    DexRegisterLocation::Kind kind = ExtractKindFromShortLocation(first_byte);
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Extract the value from the remaining 5 bits.
      int32_t value = ExtractValueFromShortLocation(first_byte);
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Convert the stack slot (short) offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    } else {
      // Large location.  Read the four next bytes to get the value.
      int32_t value = region_.LoadUnaligned<int32_t>(offset + sizeof(DexRegisterLocation::Kind));
      if (kind == DexRegisterLocation::Kind::kInStackLargeOffset) {
        // Convert the stack slot (large) offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    }
  }

  // Compute the compressed kind of `location`.
  static DexRegisterLocation::Kind ComputeCompressedKind(const DexRegisterLocation& location) {
    DexRegisterLocation::Kind kind = location.GetInternalKind();
    switch (kind) {
      case DexRegisterLocation::Kind::kInStack:
        return IsShortStackOffsetValue(location.GetValue())
            ? DexRegisterLocation::Kind::kInStack
            : DexRegisterLocation::Kind::kInStackLargeOffset;

      case DexRegisterLocation::Kind::kInRegister:
      case DexRegisterLocation::Kind::kInRegisterHigh:
        DCHECK_GE(location.GetValue(), 0);
        DCHECK_LT(location.GetValue(), 1 << kValueBits);
        return kind;

      case DexRegisterLocation::Kind::kInFpuRegister:
      case DexRegisterLocation::Kind::kInFpuRegisterHigh:
        DCHECK_GE(location.GetValue(), 0);
        DCHECK_LT(location.GetValue(), 1 << kValueBits);
        return kind;

      case DexRegisterLocation::Kind::kConstant:
        return IsShortConstantValue(location.GetValue())
            ? DexRegisterLocation::Kind::kConstant
            : DexRegisterLocation::Kind::kConstantLargeValue;

      case DexRegisterLocation::Kind::kConstantLargeValue:
      case DexRegisterLocation::Kind::kInStackLargeOffset:
      case DexRegisterLocation::Kind::kNone:
        LOG(FATAL) << "Unexpected location kind " << kind;
    }
    UNREACHABLE();
  }

  // Can `location` be turned into a short location?
  static bool CanBeEncodedAsShortLocation(const DexRegisterLocation& location) {
    DexRegisterLocation::Kind kind = location.GetInternalKind();
    switch (kind) {
      case DexRegisterLocation::Kind::kInStack:
        return IsShortStackOffsetValue(location.GetValue());

      case DexRegisterLocation::Kind::kInRegister:
      case DexRegisterLocation::Kind::kInRegisterHigh:
      case DexRegisterLocation::Kind::kInFpuRegister:
      case DexRegisterLocation::Kind::kInFpuRegisterHigh:
        return true;

      case DexRegisterLocation::Kind::kConstant:
        return IsShortConstantValue(location.GetValue());

      case DexRegisterLocation::Kind::kConstantLargeValue:
      case DexRegisterLocation::Kind::kInStackLargeOffset:
      case DexRegisterLocation::Kind::kNone:
        LOG(FATAL) << "Unexpected location kind " << kind;
    }
    UNREACHABLE();
  }

  static size_t EntrySize(const DexRegisterLocation& location) {
    return CanBeEncodedAsShortLocation(location) ? SingleShortEntrySize() : SingleLargeEntrySize();
  }

  static size_t SingleShortEntrySize() {
    return sizeof(ShortLocation);
  }

  static size_t SingleLargeEntrySize() {
    return sizeof(DexRegisterLocation::Kind) + sizeof(int32_t);
  }

  size_t Size() const {
    return region_.size();
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info);

  // Special (invalid) Dex register location catalog entry index meaning
  // that there is no location for a given Dex register (i.e., it is
  // mapped to a DexRegisterLocation::Kind::kNone location).
  static constexpr size_t kNoLocationEntryIndex = -1;

 private:
  static constexpr int kFixedSize = 0;

  // Width of the kind "field" in a short location, in bits.
  static constexpr size_t kKindBits = 3;
  // Width of the value "field" in a short location, in bits.
  static constexpr size_t kValueBits = 5;

  static constexpr uint8_t kKindMask = (1 << kKindBits) - 1;
  static constexpr int32_t kValueMask = (1 << kValueBits) - 1;
  static constexpr size_t kKindOffset = 0;
  static constexpr size_t kValueOffset = kKindBits;

  static bool IsShortStackOffsetValue(int32_t value) {
    DCHECK_EQ(value % kFrameSlotSize, 0);
    return IsShortValue(value / kFrameSlotSize);
  }

  static bool IsShortConstantValue(int32_t value) {
    return IsShortValue(value);
  }

  static bool IsShortValue(int32_t value) {
    return IsUint<kValueBits>(value);
  }

  static ShortLocation MakeShortLocation(DexRegisterLocation::Kind kind, int32_t value) {
    uint8_t kind_integer_value = static_cast<uint8_t>(kind);
    DCHECK(IsUint<kKindBits>(kind_integer_value)) << kind_integer_value;
    DCHECK(IsShortValue(value)) << value;
    return (kind_integer_value & kKindMask) << kKindOffset
        | (value & kValueMask) << kValueOffset;
  }

  static DexRegisterLocation::Kind ExtractKindFromShortLocation(ShortLocation location) {
    uint8_t kind = (location >> kKindOffset) & kKindMask;
    DCHECK_LE(kind, static_cast<uint8_t>(DexRegisterLocation::Kind::kLastLocationKind));
    // We do not encode kNone locations in the stack map.
    DCHECK_NE(kind, static_cast<uint8_t>(DexRegisterLocation::Kind::kNone));
    return static_cast<DexRegisterLocation::Kind>(kind);
  }

  static int32_t ExtractValueFromShortLocation(ShortLocation location) {
    return (location >> kValueOffset) & kValueMask;
  }

  // Extract a location kind from the byte at position `offset`.
  DexRegisterLocation::Kind ExtractKindAtOffset(size_t offset) const {
    ShortLocation first_byte = region_.LoadUnaligned<ShortLocation>(offset);
    return ExtractKindFromShortLocation(first_byte);
  }

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMapStream;
};

/* Information on Dex register locations for a specific PC, mapping a
 * stack map's Dex register to a location entry in a DexRegisterLocationCatalog.
 * The information is of the form:
 *
 *   [live_bit_mask, entries*]
 *
 * where entries are concatenated unsigned integer values encoded on a number
 * of bits (fixed per DexRegisterMap instances of a CodeInfo object) depending
 * on the number of entries in the Dex register location catalog
 * (see DexRegisterMap::SingleEntrySizeInBits).  The map is 1-byte aligned.
 */
class DexRegisterMap {
 public:
  explicit DexRegisterMap(MemoryRegion region) : region_(region) {}
  DexRegisterMap() {}

  bool IsValid() const { return region_.pointer() != nullptr; }

  // Get the surface kind of Dex register `dex_register_number`.
  DexRegisterLocation::Kind GetLocationKind(uint16_t dex_register_number,
                                            uint16_t number_of_dex_registers,
                                            const CodeInfo& code_info,
                                            const CodeInfoEncoding& enc) const {
    return DexRegisterLocation::ConvertToSurfaceKind(
        GetLocationInternalKind(dex_register_number, number_of_dex_registers, code_info, enc));
  }

  // Get the internal kind of Dex register `dex_register_number`.
  DexRegisterLocation::Kind GetLocationInternalKind(uint16_t dex_register_number,
                                                    uint16_t number_of_dex_registers,
                                                    const CodeInfo& code_info,
                                                    const CodeInfoEncoding& enc) const;

  // Get the Dex register location `dex_register_number`.
  DexRegisterLocation GetDexRegisterLocation(uint16_t dex_register_number,
                                             uint16_t number_of_dex_registers,
                                             const CodeInfo& code_info,
                                             const CodeInfoEncoding& enc) const;

  int32_t GetStackOffsetInBytes(uint16_t dex_register_number,
                                uint16_t number_of_dex_registers,
                                const CodeInfo& code_info,
                                const CodeInfoEncoding& enc) const {
    DexRegisterLocation location =
        GetDexRegisterLocation(dex_register_number, number_of_dex_registers, code_info, enc);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kInStack);
    // GetDexRegisterLocation returns the offset in bytes.
    return location.GetValue();
  }

  int32_t GetConstant(uint16_t dex_register_number,
                      uint16_t number_of_dex_registers,
                      const CodeInfo& code_info,
                      const CodeInfoEncoding& enc) const {
    DexRegisterLocation location =
        GetDexRegisterLocation(dex_register_number, number_of_dex_registers, code_info, enc);
    DCHECK_EQ(location.GetKind(), DexRegisterLocation::Kind::kConstant);
    return location.GetValue();
  }

  int32_t GetMachineRegister(uint16_t dex_register_number,
                             uint16_t number_of_dex_registers,
                             const CodeInfo& code_info,
                             const CodeInfoEncoding& enc) const {
    DexRegisterLocation location =
        GetDexRegisterLocation(dex_register_number, number_of_dex_registers, code_info, enc);
    DCHECK(location.GetInternalKind() == DexRegisterLocation::Kind::kInRegister ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInRegisterHigh ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegister ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegisterHigh)
        << location.GetInternalKind();
    return location.GetValue();
  }

  // Get the index of the entry in the Dex register location catalog
  // corresponding to `dex_register_number`.
  size_t GetLocationCatalogEntryIndex(uint16_t dex_register_number,
                                      uint16_t number_of_dex_registers,
                                      size_t number_of_location_catalog_entries) const {
    if (!IsDexRegisterLive(dex_register_number)) {
      return DexRegisterLocationCatalog::kNoLocationEntryIndex;
    }

    if (number_of_location_catalog_entries == 1) {
      // We do not allocate space for location maps in the case of a
      // single-entry location catalog, as it is useless.  The only valid
      // entry index is 0;
      return 0;
    }

    // The bit offset of the beginning of the map locations.
    size_t map_locations_offset_in_bits =
        GetLocationMappingDataOffset(number_of_dex_registers) * kBitsPerByte;
    size_t index_in_dex_register_map = GetIndexInDexRegisterMap(dex_register_number);
    DCHECK_LT(index_in_dex_register_map, GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The bit size of an entry.
    size_t map_entry_size_in_bits = SingleEntrySizeInBits(number_of_location_catalog_entries);
    // The bit offset where `index_in_dex_register_map` is located.
    size_t entry_offset_in_bits =
        map_locations_offset_in_bits + index_in_dex_register_map * map_entry_size_in_bits;
    size_t location_catalog_entry_index =
        region_.LoadBits(entry_offset_in_bits, map_entry_size_in_bits);
    DCHECK_LT(location_catalog_entry_index, number_of_location_catalog_entries);
    return location_catalog_entry_index;
  }

  // Map entry at `index_in_dex_register_map` to `location_catalog_entry_index`.
  void SetLocationCatalogEntryIndex(size_t index_in_dex_register_map,
                                    size_t location_catalog_entry_index,
                                    uint16_t number_of_dex_registers,
                                    size_t number_of_location_catalog_entries) {
    DCHECK_LT(index_in_dex_register_map, GetNumberOfLiveDexRegisters(number_of_dex_registers));
    DCHECK_LT(location_catalog_entry_index, number_of_location_catalog_entries);

    if (number_of_location_catalog_entries == 1) {
      // We do not allocate space for location maps in the case of a
      // single-entry location catalog, as it is useless.
      return;
    }

    // The bit offset of the beginning of the map locations.
    size_t map_locations_offset_in_bits =
        GetLocationMappingDataOffset(number_of_dex_registers) * kBitsPerByte;
    // The bit size of an entry.
    size_t map_entry_size_in_bits = SingleEntrySizeInBits(number_of_location_catalog_entries);
    // The bit offset where `index_in_dex_register_map` is located.
    size_t entry_offset_in_bits =
        map_locations_offset_in_bits + index_in_dex_register_map * map_entry_size_in_bits;
    region_.StoreBits(entry_offset_in_bits, location_catalog_entry_index, map_entry_size_in_bits);
  }

  void SetLiveBitMask(uint16_t number_of_dex_registers,
                      const BitVector& live_dex_registers_mask) {
    size_t live_bit_mask_offset_in_bits = GetLiveBitMaskOffset() * kBitsPerByte;
    for (uint16_t i = 0; i < number_of_dex_registers; ++i) {
      region_.StoreBit(live_bit_mask_offset_in_bits + i, live_dex_registers_mask.IsBitSet(i));
    }
  }

  ALWAYS_INLINE bool IsDexRegisterLive(uint16_t dex_register_number) const {
    size_t live_bit_mask_offset_in_bits = GetLiveBitMaskOffset() * kBitsPerByte;
    return region_.LoadBit(live_bit_mask_offset_in_bits + dex_register_number);
  }

  size_t GetNumberOfLiveDexRegisters(uint16_t number_of_dex_registers) const {
    size_t number_of_live_dex_registers = 0;
    for (size_t i = 0; i < number_of_dex_registers; ++i) {
      if (IsDexRegisterLive(i)) {
        ++number_of_live_dex_registers;
      }
    }
    return number_of_live_dex_registers;
  }

  static size_t GetLiveBitMaskOffset() {
    return kFixedSize;
  }

  // Compute the size of the live register bit mask (in bytes), for a
  // method having `number_of_dex_registers` Dex registers.
  static size_t GetLiveBitMaskSize(uint16_t number_of_dex_registers) {
    return RoundUp(number_of_dex_registers, kBitsPerByte) / kBitsPerByte;
  }

  static size_t GetLocationMappingDataOffset(uint16_t number_of_dex_registers) {
    return GetLiveBitMaskOffset() + GetLiveBitMaskSize(number_of_dex_registers);
  }

  size_t GetLocationMappingDataSize(uint16_t number_of_dex_registers,
                                    size_t number_of_location_catalog_entries) const {
    size_t location_mapping_data_size_in_bits =
        GetNumberOfLiveDexRegisters(number_of_dex_registers)
        * SingleEntrySizeInBits(number_of_location_catalog_entries);
    return RoundUp(location_mapping_data_size_in_bits, kBitsPerByte) / kBitsPerByte;
  }

  // Return the size of a map entry in bits.  Note that if
  // `number_of_location_catalog_entries` equals 1, this function returns 0,
  // which is fine, as there is no need to allocate a map for a
  // single-entry location catalog; the only valid location catalog entry index
  // for a live register in this case is 0 and there is no need to
  // store it.
  static size_t SingleEntrySizeInBits(size_t number_of_location_catalog_entries) {
    // Handle the case of 0, as we cannot pass 0 to art::WhichPowerOf2.
    return number_of_location_catalog_entries == 0
        ? 0u
        : WhichPowerOf2(RoundUpToPowerOfTwo(number_of_location_catalog_entries));
  }

  // Return the size of the DexRegisterMap object, in bytes.
  size_t Size() const {
    return region_.size();
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info, uint16_t number_of_dex_registers) const;

 private:
  // Return the index in the Dex register map corresponding to the Dex
  // register number `dex_register_number`.
  size_t GetIndexInDexRegisterMap(uint16_t dex_register_number) const {
    if (!IsDexRegisterLive(dex_register_number)) {
      return kInvalidIndexInDexRegisterMap;
    }
    return GetNumberOfLiveDexRegisters(dex_register_number);
  }

  // Special (invalid) Dex register map entry index meaning that there
  // is no index in the map for a given Dex register (i.e., it must
  // have been mapped to a DexRegisterLocation::Kind::kNone location).
  static constexpr size_t kInvalidIndexInDexRegisterMap = -1;

  static constexpr int kFixedSize = 0;

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMapStream;
};

// Represents bit range of bit-packed integer field.
// We reuse the idea from ULEB128p1 to support encoding of -1 (aka 0xFFFFFFFF).
// If min_value is set to -1, we implicitly subtract one from any loaded value,
// and add one to any stored value. This is generalized to any negative values.
// In other words, min_value acts as a base and the stored value is added to it.
struct FieldEncoding {
  FieldEncoding(size_t start_offset, size_t end_offset, int32_t min_value = 0)
      : start_offset_(start_offset), end_offset_(end_offset), min_value_(min_value) {
    DCHECK_LE(start_offset_, end_offset_);
    DCHECK_LE(BitSize(), 32u);
  }

  ALWAYS_INLINE size_t BitSize() const { return end_offset_ - start_offset_; }

  template <typename Region>
  ALWAYS_INLINE int32_t Load(const Region& region) const {
    DCHECK_LE(end_offset_, region.size_in_bits());
    return static_cast<int32_t>(region.LoadBits(start_offset_, BitSize())) + min_value_;
  }

  template <typename Region>
  ALWAYS_INLINE void Store(Region region, int32_t value) const {
    region.StoreBits(start_offset_, value - min_value_, BitSize());
    DCHECK_EQ(Load(region), value);
  }

 private:
  size_t start_offset_;
  size_t end_offset_;
  int32_t min_value_;
};

class StackMapEncoding {
 public:
  StackMapEncoding()
      : dex_pc_bit_offset_(0),
        dex_register_map_bit_offset_(0),
        inline_info_bit_offset_(0),
        register_mask_index_bit_offset_(0),
        stack_mask_index_bit_offset_(0),
        total_bit_size_(0) {}

  // Set stack map bit layout based on given sizes.
  // Returns the size of stack map in bits.
  size_t SetFromSizes(size_t native_pc_max,
                      size_t dex_pc_max,
                      size_t dex_register_map_size,
                      size_t number_of_inline_info,
                      size_t number_of_register_masks,
                      size_t number_of_stack_masks) {
    total_bit_size_ = 0;
    DCHECK_EQ(kNativePcBitOffset, total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(native_pc_max);

    dex_pc_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(1 /* kNoDexPc */ + dex_pc_max);

    // We also need +1 for kNoDexRegisterMap, but since the size is strictly
    // greater than any offset we might try to encode, we already implicitly have it.
    dex_register_map_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(dex_register_map_size);

    // We also need +1 for kNoInlineInfo, but since the inline_info_size is strictly
    // greater than the offset we might try to encode, we already implicitly have it.
    // If inline_info_size is zero, we can encode only kNoInlineInfo (in zero bits).
    inline_info_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(number_of_inline_info);

    register_mask_index_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(number_of_register_masks);

    stack_mask_index_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(number_of_stack_masks);

    return total_bit_size_;
  }

  ALWAYS_INLINE FieldEncoding GetNativePcEncoding() const {
    return FieldEncoding(kNativePcBitOffset, dex_pc_bit_offset_);
  }
  ALWAYS_INLINE FieldEncoding GetDexPcEncoding() const {
    return FieldEncoding(dex_pc_bit_offset_, dex_register_map_bit_offset_, -1 /* min_value */);
  }
  ALWAYS_INLINE FieldEncoding GetDexRegisterMapEncoding() const {
    return FieldEncoding(dex_register_map_bit_offset_, inline_info_bit_offset_, -1 /* min_value */);
  }
  ALWAYS_INLINE FieldEncoding GetInlineInfoEncoding() const {
    return FieldEncoding(inline_info_bit_offset_,
                         register_mask_index_bit_offset_,
                         -1 /* min_value */);
  }
  ALWAYS_INLINE FieldEncoding GetRegisterMaskIndexEncoding() const {
    return FieldEncoding(register_mask_index_bit_offset_, stack_mask_index_bit_offset_);
  }
  ALWAYS_INLINE FieldEncoding GetStackMaskIndexEncoding() const {
    return FieldEncoding(stack_mask_index_bit_offset_, total_bit_size_);
  }
  ALWAYS_INLINE size_t BitSize() const {
    return total_bit_size_;
  }

  // Encode the encoding into the vector.
  template<typename Vector>
  void Encode(Vector* dest) const {
    static_assert(alignof(StackMapEncoding) == 1, "Should not require alignment");
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(this);
    dest->insert(dest->end(), ptr, ptr + sizeof(*this));
  }

  // Decode the encoding from a pointer, updates the pointer.
  void Decode(const uint8_t** ptr) {
    *this = *reinterpret_cast<const StackMapEncoding*>(*ptr);
    *ptr += sizeof(*this);
  }

  void Dump(VariableIndentationOutputStream* vios) const;

 private:
  static constexpr size_t kNativePcBitOffset = 0;
  uint8_t dex_pc_bit_offset_;
  uint8_t dex_register_map_bit_offset_;
  uint8_t inline_info_bit_offset_;
  uint8_t register_mask_index_bit_offset_;
  uint8_t stack_mask_index_bit_offset_;
  uint8_t total_bit_size_;
};

/**
 * A Stack Map holds compilation information for a specific PC necessary for:
 * - Mapping it to a dex PC,
 * - Knowing which stack entries are objects,
 * - Knowing which registers hold objects,
 * - Knowing the inlining information,
 * - Knowing the values of dex registers.
 *
 * The information is of the form:
 *
 *   [native_pc_offset, dex_pc, dex_register_map_offset, inlining_info_index, register_mask_index,
 *   stack_mask_index].
 */
class StackMap {
 public:
  StackMap() {}
  explicit StackMap(BitMemoryRegion region) : region_(region) {}

  ALWAYS_INLINE bool IsValid() const { return region_.pointer() != nullptr; }

  ALWAYS_INLINE uint32_t GetDexPc(const StackMapEncoding& encoding) const {
    return encoding.GetDexPcEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetDexPc(const StackMapEncoding& encoding, uint32_t dex_pc) {
    encoding.GetDexPcEncoding().Store(region_, dex_pc);
  }

  ALWAYS_INLINE uint32_t GetNativePcOffset(const StackMapEncoding& encoding,
                                           InstructionSet instruction_set) const {
    CodeOffset offset(
        CodeOffset::FromCompressedOffset(encoding.GetNativePcEncoding().Load(region_)));
    return offset.Uint32Value(instruction_set);
  }

  ALWAYS_INLINE void SetNativePcCodeOffset(const StackMapEncoding& encoding,
                                           CodeOffset native_pc_offset) {
    encoding.GetNativePcEncoding().Store(region_, native_pc_offset.CompressedValue());
  }

  ALWAYS_INLINE uint32_t GetDexRegisterMapOffset(const StackMapEncoding& encoding) const {
    return encoding.GetDexRegisterMapEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetDexRegisterMapOffset(const StackMapEncoding& encoding, uint32_t offset) {
    encoding.GetDexRegisterMapEncoding().Store(region_, offset);
  }

  ALWAYS_INLINE uint32_t GetInlineInfoIndex(const StackMapEncoding& encoding) const {
    return encoding.GetInlineInfoEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetInlineInfoIndex(const StackMapEncoding& encoding, uint32_t index) {
    encoding.GetInlineInfoEncoding().Store(region_, index);
  }

  ALWAYS_INLINE uint32_t GetRegisterMaskIndex(const StackMapEncoding& encoding) const {
    return encoding.GetRegisterMaskIndexEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetRegisterMaskIndex(const StackMapEncoding& encoding, uint32_t mask) {
    encoding.GetRegisterMaskIndexEncoding().Store(region_, mask);
  }

  ALWAYS_INLINE uint32_t GetStackMaskIndex(const StackMapEncoding& encoding) const {
    return encoding.GetStackMaskIndexEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetStackMaskIndex(const StackMapEncoding& encoding, uint32_t mask) {
    encoding.GetStackMaskIndexEncoding().Store(region_, mask);
  }

  ALWAYS_INLINE bool HasDexRegisterMap(const StackMapEncoding& encoding) const {
    return GetDexRegisterMapOffset(encoding) != kNoDexRegisterMap;
  }

  ALWAYS_INLINE bool HasInlineInfo(const StackMapEncoding& encoding) const {
    return GetInlineInfoIndex(encoding) != kNoInlineInfo;
  }

  ALWAYS_INLINE bool Equals(const StackMap& other) const {
    return region_.pointer() == other.region_.pointer() &&
           region_.size() == other.region_.size() &&
           region_.BitOffset() == other.region_.BitOffset();
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info,
            const CodeInfoEncoding& encoding,
            const MethodInfo& method_info,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            InstructionSet instruction_set,
            const std::string& header_suffix = "") const;

  // Special (invalid) offset for the DexRegisterMapOffset field meaning
  // that there is no Dex register map for this stack map.
  static constexpr uint32_t kNoDexRegisterMap = -1;

  // Special (invalid) offset for the InlineDescriptorOffset field meaning
  // that there is no inline info for this stack map.
  static constexpr uint32_t kNoInlineInfo = -1;

 private:
  static constexpr int kFixedSize = 0;

  BitMemoryRegion region_;

  friend class StackMapStream;
};

class InlineInfoEncoding {
 public:
  void SetFromSizes(size_t method_index_idx_max,
                    size_t dex_pc_max,
                    size_t extra_data_max,
                    size_t dex_register_map_size) {
    total_bit_size_ = kMethodIndexBitOffset;
    total_bit_size_ += MinimumBitsToStore(method_index_idx_max);

    dex_pc_bit_offset_ = dchecked_integral_cast<uint8_t>(total_bit_size_);
    // Note: We're not encoding the dex pc if there is none. That's the case
    // for an intrinsified native method, such as String.charAt().
    if (dex_pc_max != DexFile::kDexNoIndex) {
      total_bit_size_ += MinimumBitsToStore(1 /* kNoDexPc */ + dex_pc_max);
    }

    extra_data_bit_offset_ = dchecked_integral_cast<uint8_t>(total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(extra_data_max);

    // We also need +1 for kNoDexRegisterMap, but since the size is strictly
    // greater than any offset we might try to encode, we already implicitly have it.
    dex_register_map_bit_offset_ = dchecked_integral_cast<uint8_t>(total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(dex_register_map_size);
  }

  ALWAYS_INLINE FieldEncoding GetMethodIndexIdxEncoding() const {
    return FieldEncoding(kMethodIndexBitOffset, dex_pc_bit_offset_);
  }
  ALWAYS_INLINE FieldEncoding GetDexPcEncoding() const {
    return FieldEncoding(dex_pc_bit_offset_, extra_data_bit_offset_, -1 /* min_value */);
  }
  ALWAYS_INLINE FieldEncoding GetExtraDataEncoding() const {
    return FieldEncoding(extra_data_bit_offset_, dex_register_map_bit_offset_);
  }
  ALWAYS_INLINE FieldEncoding GetDexRegisterMapEncoding() const {
    return FieldEncoding(dex_register_map_bit_offset_, total_bit_size_, -1 /* min_value */);
  }
  ALWAYS_INLINE size_t BitSize() const {
    return total_bit_size_;
  }

  void Dump(VariableIndentationOutputStream* vios) const;

  // Encode the encoding into the vector.
  template<typename Vector>
  void Encode(Vector* dest) const {
    static_assert(alignof(InlineInfoEncoding) == 1, "Should not require alignment");
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(this);
    dest->insert(dest->end(), ptr, ptr + sizeof(*this));
  }

  // Decode the encoding from a pointer, updates the pointer.
  void Decode(const uint8_t** ptr) {
    *this = *reinterpret_cast<const InlineInfoEncoding*>(*ptr);
    *ptr += sizeof(*this);
  }

 private:
  static constexpr uint8_t kIsLastBitOffset = 0;
  static constexpr uint8_t kMethodIndexBitOffset = 1;
  uint8_t dex_pc_bit_offset_;
  uint8_t extra_data_bit_offset_;
  uint8_t dex_register_map_bit_offset_;
  uint8_t total_bit_size_;
};

/**
 * Inline information for a specific PC. The information is of the form:
 *
 *   [is_last,
 *    method_index (or ArtMethod high bits),
 *    dex_pc,
 *    extra_data (ArtMethod low bits or 1),
 *    dex_register_map_offset]+.
 */
class InlineInfo {
 public:
  explicit InlineInfo(BitMemoryRegion region) : region_(region) {}

  ALWAYS_INLINE uint32_t GetDepth(const InlineInfoEncoding& encoding) const {
    size_t depth = 0;
    while (!GetRegionAtDepth(encoding, depth++).LoadBit(0)) { }  // Check is_last bit.
    return depth;
  }

  ALWAYS_INLINE void SetDepth(const InlineInfoEncoding& encoding, uint32_t depth) {
    DCHECK_GT(depth, 0u);
    for (size_t d = 0; d < depth; ++d) {
      GetRegionAtDepth(encoding, d).StoreBit(0, d == depth - 1);  // Set is_last bit.
    }
  }

  ALWAYS_INLINE uint32_t GetMethodIndexIdxAtDepth(const InlineInfoEncoding& encoding,
                                                  uint32_t depth) const {
    DCHECK(!EncodesArtMethodAtDepth(encoding, depth));
    return encoding.GetMethodIndexIdxEncoding().Load(GetRegionAtDepth(encoding, depth));
  }

  ALWAYS_INLINE void SetMethodIndexIdxAtDepth(const InlineInfoEncoding& encoding,
                                              uint32_t depth,
                                              uint32_t index) {
    encoding.GetMethodIndexIdxEncoding().Store(GetRegionAtDepth(encoding, depth), index);
  }


  ALWAYS_INLINE uint32_t GetMethodIndexAtDepth(const InlineInfoEncoding& encoding,
                                               const MethodInfo& method_info,
                                               uint32_t depth) const {
    return method_info.GetMethodIndex(GetMethodIndexIdxAtDepth(encoding, depth));
  }

  ALWAYS_INLINE uint32_t GetDexPcAtDepth(const InlineInfoEncoding& encoding,
                                         uint32_t depth) const {
    return encoding.GetDexPcEncoding().Load(GetRegionAtDepth(encoding, depth));
  }

  ALWAYS_INLINE void SetDexPcAtDepth(const InlineInfoEncoding& encoding,
                                     uint32_t depth,
                                     uint32_t dex_pc) {
    encoding.GetDexPcEncoding().Store(GetRegionAtDepth(encoding, depth), dex_pc);
  }

  ALWAYS_INLINE bool EncodesArtMethodAtDepth(const InlineInfoEncoding& encoding,
                                             uint32_t depth) const {
    return (encoding.GetExtraDataEncoding().Load(GetRegionAtDepth(encoding, depth)) & 1) == 0;
  }

  ALWAYS_INLINE void SetExtraDataAtDepth(const InlineInfoEncoding& encoding,
                                         uint32_t depth,
                                         uint32_t extra_data) {
    encoding.GetExtraDataEncoding().Store(GetRegionAtDepth(encoding, depth), extra_data);
  }

  ALWAYS_INLINE ArtMethod* GetArtMethodAtDepth(const InlineInfoEncoding& encoding,
                                               uint32_t depth) const {
    uint32_t low_bits = encoding.GetExtraDataEncoding().Load(GetRegionAtDepth(encoding, depth));
    uint32_t high_bits = encoding.GetMethodIndexIdxEncoding().Load(
        GetRegionAtDepth(encoding, depth));
    if (high_bits == 0) {
      return reinterpret_cast<ArtMethod*>(low_bits);
    } else {
      uint64_t address = high_bits;
      address = address << 32;
      return reinterpret_cast<ArtMethod*>(address | low_bits);
    }
  }

  ALWAYS_INLINE uint32_t GetDexRegisterMapOffsetAtDepth(const InlineInfoEncoding& encoding,
                                                        uint32_t depth) const {
    return encoding.GetDexRegisterMapEncoding().Load(GetRegionAtDepth(encoding, depth));
  }

  ALWAYS_INLINE void SetDexRegisterMapOffsetAtDepth(const InlineInfoEncoding& encoding,
                                                    uint32_t depth,
                                                    uint32_t offset) {
    encoding.GetDexRegisterMapEncoding().Store(GetRegionAtDepth(encoding, depth), offset);
  }

  ALWAYS_INLINE bool HasDexRegisterMapAtDepth(const InlineInfoEncoding& encoding,
                                              uint32_t depth) const {
    return GetDexRegisterMapOffsetAtDepth(encoding, depth) != StackMap::kNoDexRegisterMap;
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& info,
            const MethodInfo& method_info,
            uint16_t* number_of_dex_registers) const;

 private:
  ALWAYS_INLINE BitMemoryRegion GetRegionAtDepth(const InlineInfoEncoding& encoding,
                                                 uint32_t depth) const {
    size_t entry_size = encoding.BitSize();
    DCHECK_GT(entry_size, 0u);
    return region_.Subregion(depth * entry_size, entry_size);
  }

  BitMemoryRegion region_;
};

// Bit sized region encoding, may be more than 255 bits.
class BitRegionEncoding {
 public:
  uint32_t num_bits = 0;

  ALWAYS_INLINE size_t BitSize() const {
    return num_bits;
  }

  template<typename Vector>
  void Encode(Vector* dest) const {
    EncodeUnsignedLeb128(dest, num_bits);  // Use leb in case num_bits is greater than 255.
  }

  void Decode(const uint8_t** ptr) {
    num_bits = DecodeUnsignedLeb128(ptr);
  }
};

// A table of bit sized encodings.
template <typename Encoding>
struct BitEncodingTable {
  static constexpr size_t kInvalidOffset = static_cast<size_t>(-1);
  // How the encoding is laid out (serialized).
  Encoding encoding;

  // Number of entries in the table (serialized).
  size_t num_entries;

  // Bit offset for the base of the table (computed).
  size_t bit_offset = kInvalidOffset;

  template<typename Vector>
  void Encode(Vector* dest) const {
    EncodeUnsignedLeb128(dest, num_entries);
    encoding.Encode(dest);
  }

  ALWAYS_INLINE void Decode(const uint8_t** ptr) {
    num_entries = DecodeUnsignedLeb128(ptr);
    encoding.Decode(ptr);
  }

  // Set the bit offset in the table and adds the space used by the table to offset.
  void UpdateBitOffset(size_t* offset) {
    DCHECK(offset != nullptr);
    bit_offset = *offset;
    *offset += encoding.BitSize() * num_entries;
  }

  // Return the bit region for the map at index i.
  ALWAYS_INLINE BitMemoryRegion BitRegion(MemoryRegion region, size_t index) const {
    DCHECK_NE(bit_offset, kInvalidOffset) << "Invalid table offset";
    DCHECK_LT(index, num_entries);
    const size_t map_size = encoding.BitSize();
    return BitMemoryRegion(region, bit_offset + index * map_size, map_size);
  }
};

// A byte sized table of possible variable sized encodings.
struct ByteSizedTable {
  static constexpr size_t kInvalidOffset = static_cast<size_t>(-1);

  // Number of entries in the table (serialized).
  size_t num_entries = 0;

  // Number of bytes of the table (serialized).
  size_t num_bytes;

  // Bit offset for the base of the table (computed).
  size_t byte_offset = kInvalidOffset;

  template<typename Vector>
  void Encode(Vector* dest) const {
    EncodeUnsignedLeb128(dest, num_entries);
    EncodeUnsignedLeb128(dest, num_bytes);
  }

  ALWAYS_INLINE void Decode(const uint8_t** ptr) {
    num_entries = DecodeUnsignedLeb128(ptr);
    num_bytes = DecodeUnsignedLeb128(ptr);
  }

  // Set the bit offset of the table. Adds the total bit size of the table to offset.
  void UpdateBitOffset(size_t* offset) {
    DCHECK(offset != nullptr);
    DCHECK_ALIGNED(*offset, kBitsPerByte);
    byte_offset = *offset / kBitsPerByte;
    *offset += num_bytes * kBitsPerByte;
  }
};

// Format is [native pc, invoke type, method index].
class InvokeInfoEncoding {
 public:
  void SetFromSizes(size_t native_pc_max,
                    size_t invoke_type_max,
                    size_t method_index_max) {
    total_bit_size_ = 0;
    DCHECK_EQ(kNativePcBitOffset, total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(native_pc_max);
    invoke_type_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(invoke_type_max);
    method_index_bit_offset_ = total_bit_size_;
    total_bit_size_ += MinimumBitsToStore(method_index_max);
  }

  ALWAYS_INLINE FieldEncoding GetNativePcEncoding() const {
    return FieldEncoding(kNativePcBitOffset, invoke_type_bit_offset_);
  }

  ALWAYS_INLINE FieldEncoding GetInvokeTypeEncoding() const {
    return FieldEncoding(invoke_type_bit_offset_, method_index_bit_offset_);
  }

  ALWAYS_INLINE FieldEncoding GetMethodIndexEncoding() const {
    return FieldEncoding(method_index_bit_offset_, total_bit_size_);
  }

  ALWAYS_INLINE size_t BitSize() const {
    return total_bit_size_;
  }

  template<typename Vector>
  void Encode(Vector* dest) const {
    static_assert(alignof(InvokeInfoEncoding) == 1, "Should not require alignment");
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(this);
    dest->insert(dest->end(), ptr, ptr + sizeof(*this));
  }

  void Decode(const uint8_t** ptr) {
    *this = *reinterpret_cast<const InvokeInfoEncoding*>(*ptr);
    *ptr += sizeof(*this);
  }

 private:
  static constexpr uint8_t kNativePcBitOffset = 0;
  uint8_t invoke_type_bit_offset_;
  uint8_t method_index_bit_offset_;
  uint8_t total_bit_size_;
};

class InvokeInfo {
 public:
  explicit InvokeInfo(BitMemoryRegion region) : region_(region) {}

  ALWAYS_INLINE uint32_t GetNativePcOffset(const InvokeInfoEncoding& encoding,
                                           InstructionSet instruction_set) const {
    CodeOffset offset(
        CodeOffset::FromCompressedOffset(encoding.GetNativePcEncoding().Load(region_)));
    return offset.Uint32Value(instruction_set);
  }

  ALWAYS_INLINE void SetNativePcCodeOffset(const InvokeInfoEncoding& encoding,
                                           CodeOffset native_pc_offset) {
    encoding.GetNativePcEncoding().Store(region_, native_pc_offset.CompressedValue());
  }

  ALWAYS_INLINE uint32_t GetInvokeType(const InvokeInfoEncoding& encoding) const {
    return encoding.GetInvokeTypeEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetInvokeType(const InvokeInfoEncoding& encoding, uint32_t invoke_type) {
    encoding.GetInvokeTypeEncoding().Store(region_, invoke_type);
  }

  ALWAYS_INLINE uint32_t GetMethodIndexIdx(const InvokeInfoEncoding& encoding) const {
    return encoding.GetMethodIndexEncoding().Load(region_);
  }

  ALWAYS_INLINE void SetMethodIndexIdx(const InvokeInfoEncoding& encoding,
                                       uint32_t method_index_idx) {
    encoding.GetMethodIndexEncoding().Store(region_, method_index_idx);
  }

  ALWAYS_INLINE uint32_t GetMethodIndex(const InvokeInfoEncoding& encoding,
                                        MethodInfo method_info) const {
    return method_info.GetMethodIndex(GetMethodIndexIdx(encoding));
  }

  bool IsValid() const { return region_.pointer() != nullptr; }

 private:
  BitMemoryRegion region_;
};

// Most of the fields are encoded as ULEB128 to save space.
struct CodeInfoEncoding {
  static constexpr uint32_t kInvalidSize = static_cast<size_t>(-1);
  // Byte sized tables go first to avoid unnecessary alignment bits.
  ByteSizedTable dex_register_map;
  ByteSizedTable location_catalog;
  BitEncodingTable<StackMapEncoding> stack_map;
  BitEncodingTable<BitRegionEncoding> register_mask;
  BitEncodingTable<BitRegionEncoding> stack_mask;
  BitEncodingTable<InvokeInfoEncoding> invoke_info;
  BitEncodingTable<InlineInfoEncoding> inline_info;

  CodeInfoEncoding() {}

  explicit CodeInfoEncoding(const void* data) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
    dex_register_map.Decode(&ptr);
    location_catalog.Decode(&ptr);
    stack_map.Decode(&ptr);
    register_mask.Decode(&ptr);
    stack_mask.Decode(&ptr);
    invoke_info.Decode(&ptr);
    if (stack_map.encoding.GetInlineInfoEncoding().BitSize() > 0) {
      inline_info.Decode(&ptr);
    } else {
      inline_info = BitEncodingTable<InlineInfoEncoding>();
    }
    cache_header_size =
        dchecked_integral_cast<uint32_t>(ptr - reinterpret_cast<const uint8_t*>(data));
    ComputeTableOffsets();
  }

  // Compress is not const since it calculates cache_header_size. This is used by PrepareForFillIn.
  template<typename Vector>
  void Compress(Vector* dest) {
    dex_register_map.Encode(dest);
    location_catalog.Encode(dest);
    stack_map.Encode(dest);
    register_mask.Encode(dest);
    stack_mask.Encode(dest);
    invoke_info.Encode(dest);
    if (stack_map.encoding.GetInlineInfoEncoding().BitSize() > 0) {
      inline_info.Encode(dest);
    }
    cache_header_size = dest->size();
  }

  ALWAYS_INLINE void ComputeTableOffsets() {
    // Skip the header.
    size_t bit_offset = HeaderSize() * kBitsPerByte;
    // The byte tables must be aligned so they must go first.
    dex_register_map.UpdateBitOffset(&bit_offset);
    location_catalog.UpdateBitOffset(&bit_offset);
    // Other tables don't require alignment.
    stack_map.UpdateBitOffset(&bit_offset);
    register_mask.UpdateBitOffset(&bit_offset);
    stack_mask.UpdateBitOffset(&bit_offset);
    invoke_info.UpdateBitOffset(&bit_offset);
    inline_info.UpdateBitOffset(&bit_offset);
    cache_non_header_size = RoundUp(bit_offset, kBitsPerByte) / kBitsPerByte - HeaderSize();
  }

  ALWAYS_INLINE size_t HeaderSize() const {
    DCHECK_NE(cache_header_size, kInvalidSize) << "Uninitialized";
    return cache_header_size;
  }

  ALWAYS_INLINE size_t NonHeaderSize() const {
    DCHECK_NE(cache_non_header_size, kInvalidSize) << "Uninitialized";
    return cache_non_header_size;
  }

 private:
  // Computed fields (not serialized).
  // Header size in bytes, cached to avoid needing to re-decoding the encoding in HeaderSize.
  uint32_t cache_header_size = kInvalidSize;
  // Non header size in bytes, cached to avoid needing to re-decoding the encoding in NonHeaderSize.
  uint32_t cache_non_header_size = kInvalidSize;
};

/**
 * Wrapper around all compiler information collected for a method.
 * The information is of the form:
 *
 *   [CodeInfoEncoding, DexRegisterMap+, DexLocationCatalog+, StackMap+, RegisterMask+, StackMask+,
 *    InlineInfo*]
 *
 * where CodeInfoEncoding is of the form:
 *
 *   [ByteSizedTable(dex_register_map), ByteSizedTable(location_catalog),
 *    BitEncodingTable<StackMapEncoding>, BitEncodingTable<BitRegionEncoding>,
 *    BitEncodingTable<BitRegionEncoding>, BitEncodingTable<InlineInfoEncoding>]
 */
class CodeInfo {
 public:
  explicit CodeInfo(MemoryRegion region) : region_(region) {
  }

  explicit CodeInfo(const void* data) {
    CodeInfoEncoding encoding = CodeInfoEncoding(data);
    region_ = MemoryRegion(const_cast<void*>(data),
                           encoding.HeaderSize() + encoding.NonHeaderSize());
  }

  CodeInfoEncoding ExtractEncoding() const {
    CodeInfoEncoding encoding(region_.begin());
    AssertValidStackMap(encoding);
    return encoding;
  }

  bool HasInlineInfo(const CodeInfoEncoding& encoding) const {
    return encoding.stack_map.encoding.GetInlineInfoEncoding().BitSize() > 0;
  }

  DexRegisterLocationCatalog GetDexRegisterLocationCatalog(const CodeInfoEncoding& encoding) const {
    return DexRegisterLocationCatalog(region_.Subregion(encoding.location_catalog.byte_offset,
                                                        encoding.location_catalog.num_bytes));
  }

  ALWAYS_INLINE size_t GetNumberOfStackMaskBits(const CodeInfoEncoding& encoding) const {
    return encoding.stack_mask.encoding.BitSize();
  }

  ALWAYS_INLINE StackMap GetStackMapAt(size_t index, const CodeInfoEncoding& encoding) const {
    return StackMap(encoding.stack_map.BitRegion(region_, index));
  }

  BitMemoryRegion GetStackMask(size_t index, const CodeInfoEncoding& encoding) const {
    return encoding.stack_mask.BitRegion(region_, index);
  }

  BitMemoryRegion GetStackMaskOf(const CodeInfoEncoding& encoding,
                                 const StackMap& stack_map) const {
    return GetStackMask(stack_map.GetStackMaskIndex(encoding.stack_map.encoding), encoding);
  }

  BitMemoryRegion GetRegisterMask(size_t index, const CodeInfoEncoding& encoding) const {
    return encoding.register_mask.BitRegion(region_, index);
  }

  uint32_t GetRegisterMaskOf(const CodeInfoEncoding& encoding, const StackMap& stack_map) const {
    size_t index = stack_map.GetRegisterMaskIndex(encoding.stack_map.encoding);
    return GetRegisterMask(index, encoding).LoadBits(0u, encoding.register_mask.encoding.BitSize());
  }

  uint32_t GetNumberOfLocationCatalogEntries(const CodeInfoEncoding& encoding) const {
    return encoding.location_catalog.num_entries;
  }

  uint32_t GetDexRegisterLocationCatalogSize(const CodeInfoEncoding& encoding) const {
    return encoding.location_catalog.num_bytes;
  }

  uint32_t GetNumberOfStackMaps(const CodeInfoEncoding& encoding) const {
    return encoding.stack_map.num_entries;
  }

  // Get the size of all the stack maps of this CodeInfo object, in bits. Not byte aligned.
  ALWAYS_INLINE size_t GetStackMapsSizeInBits(const CodeInfoEncoding& encoding) const {
    return encoding.stack_map.encoding.BitSize() * GetNumberOfStackMaps(encoding);
  }

  InvokeInfo GetInvokeInfo(const CodeInfoEncoding& encoding, size_t index) const {
    return InvokeInfo(encoding.invoke_info.BitRegion(region_, index));
  }

  DexRegisterMap GetDexRegisterMapOf(StackMap stack_map,
                                     const CodeInfoEncoding& encoding,
                                     size_t number_of_dex_registers) const {
    if (!stack_map.HasDexRegisterMap(encoding.stack_map.encoding)) {
      return DexRegisterMap();
    }
    const uint32_t offset = encoding.dex_register_map.byte_offset +
        stack_map.GetDexRegisterMapOffset(encoding.stack_map.encoding);
    size_t size = ComputeDexRegisterMapSizeOf(encoding, offset, number_of_dex_registers);
    return DexRegisterMap(region_.Subregion(offset, size));
  }

  size_t GetDexRegisterMapsSize(const CodeInfoEncoding& encoding,
                                uint32_t number_of_dex_registers) const {
    size_t total = 0;
    for (size_t i = 0, e = GetNumberOfStackMaps(encoding); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i, encoding);
      DexRegisterMap map(GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers));
      total += map.Size();
    }
    return total;
  }

  // Return the `DexRegisterMap` pointed by `inline_info` at depth `depth`.
  DexRegisterMap GetDexRegisterMapAtDepth(uint8_t depth,
                                          InlineInfo inline_info,
                                          const CodeInfoEncoding& encoding,
                                          uint32_t number_of_dex_registers) const {
    if (!inline_info.HasDexRegisterMapAtDepth(encoding.inline_info.encoding, depth)) {
      return DexRegisterMap();
    } else {
      uint32_t offset = encoding.dex_register_map.byte_offset +
          inline_info.GetDexRegisterMapOffsetAtDepth(encoding.inline_info.encoding, depth);
      size_t size = ComputeDexRegisterMapSizeOf(encoding, offset, number_of_dex_registers);
      return DexRegisterMap(region_.Subregion(offset, size));
    }
  }

  InlineInfo GetInlineInfo(size_t index, const CodeInfoEncoding& encoding) const {
    // Since we do not know the depth, we just return the whole remaining map. The caller may
    // access the inline info for arbitrary depths. To return the precise inline info we would need
    // to count the depth before returning.
    // TODO: Clean this up.
    const size_t bit_offset = encoding.inline_info.bit_offset +
        index * encoding.inline_info.encoding.BitSize();
    return InlineInfo(BitMemoryRegion(region_, bit_offset, region_.size_in_bits() - bit_offset));
  }

  InlineInfo GetInlineInfoOf(StackMap stack_map, const CodeInfoEncoding& encoding) const {
    DCHECK(stack_map.HasInlineInfo(encoding.stack_map.encoding));
    uint32_t index = stack_map.GetInlineInfoIndex(encoding.stack_map.encoding);
    return GetInlineInfo(index, encoding);
  }

  StackMap GetStackMapForDexPc(uint32_t dex_pc, const CodeInfoEncoding& encoding) const {
    for (size_t i = 0, e = GetNumberOfStackMaps(encoding); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i, encoding);
      if (stack_map.GetDexPc(encoding.stack_map.encoding) == dex_pc) {
        return stack_map;
      }
    }
    return StackMap();
  }

  // Searches the stack map list backwards because catch stack maps are stored
  // at the end.
  StackMap GetCatchStackMapForDexPc(uint32_t dex_pc, const CodeInfoEncoding& encoding) const {
    for (size_t i = GetNumberOfStackMaps(encoding); i > 0; --i) {
      StackMap stack_map = GetStackMapAt(i - 1, encoding);
      if (stack_map.GetDexPc(encoding.stack_map.encoding) == dex_pc) {
        return stack_map;
      }
    }
    return StackMap();
  }

  StackMap GetOsrStackMapForDexPc(uint32_t dex_pc, const CodeInfoEncoding& encoding) const {
    size_t e = GetNumberOfStackMaps(encoding);
    if (e == 0) {
      // There cannot be OSR stack map if there is no stack map.
      return StackMap();
    }
    // Walk over all stack maps. If two consecutive stack maps are identical, then we
    // have found a stack map suitable for OSR.
    const StackMapEncoding& stack_map_encoding = encoding.stack_map.encoding;
    for (size_t i = 0; i < e - 1; ++i) {
      StackMap stack_map = GetStackMapAt(i, encoding);
      if (stack_map.GetDexPc(stack_map_encoding) == dex_pc) {
        StackMap other = GetStackMapAt(i + 1, encoding);
        if (other.GetDexPc(stack_map_encoding) == dex_pc &&
            other.GetNativePcOffset(stack_map_encoding, kRuntimeISA) ==
                stack_map.GetNativePcOffset(stack_map_encoding, kRuntimeISA)) {
          DCHECK_EQ(other.GetDexRegisterMapOffset(stack_map_encoding),
                    stack_map.GetDexRegisterMapOffset(stack_map_encoding));
          DCHECK(!stack_map.HasInlineInfo(stack_map_encoding));
          if (i < e - 2) {
            // Make sure there are not three identical stack maps following each other.
            DCHECK_NE(
                stack_map.GetNativePcOffset(stack_map_encoding, kRuntimeISA),
                GetStackMapAt(i + 2, encoding).GetNativePcOffset(stack_map_encoding, kRuntimeISA));
          }
          return stack_map;
        }
      }
    }
    return StackMap();
  }

  StackMap GetStackMapForNativePcOffset(uint32_t native_pc_offset,
                                        const CodeInfoEncoding& encoding) const {
    // TODO: Safepoint stack maps are sorted by native_pc_offset but catch stack
    //       maps are not. If we knew that the method does not have try/catch,
    //       we could do binary search.
    for (size_t i = 0, e = GetNumberOfStackMaps(encoding); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i, encoding);
      if (stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA) ==
          native_pc_offset) {
        return stack_map;
      }
    }
    return StackMap();
  }

  InvokeInfo GetInvokeInfoForNativePcOffset(uint32_t native_pc_offset,
                                            const CodeInfoEncoding& encoding) {
    for (size_t index = 0; index < encoding.invoke_info.num_entries; index++) {
      InvokeInfo item = GetInvokeInfo(encoding, index);
      if (item.GetNativePcOffset(encoding.invoke_info.encoding, kRuntimeISA) == native_pc_offset) {
        return item;
      }
    }
    return InvokeInfo(BitMemoryRegion());
  }

  // Dump this CodeInfo object on `os`.  `code_offset` is the (absolute)
  // native PC of the compiled method and `number_of_dex_registers` the
  // number of Dex virtual registers used in this method.  If
  // `dump_stack_maps` is true, also dump the stack maps and the
  // associated Dex register maps.
  void Dump(VariableIndentationOutputStream* vios,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            bool dump_stack_maps,
            InstructionSet instruction_set,
            const MethodInfo& method_info) const;

  // Check that the code info has valid stack map and abort if it does not.
  void AssertValidStackMap(const CodeInfoEncoding& encoding) const {
    if (region_.size() != 0 && region_.size_in_bits() < GetStackMapsSizeInBits(encoding)) {
      LOG(FATAL) << region_.size() << "\n"
                 << encoding.HeaderSize() << "\n"
                 << encoding.NonHeaderSize() << "\n"
                 << encoding.location_catalog.num_entries << "\n"
                 << encoding.stack_map.num_entries << "\n"
                 << encoding.stack_map.encoding.BitSize();
    }
  }

 private:
  // Compute the size of the Dex register map associated to the stack map at
  // `dex_register_map_offset_in_code_info`.
  size_t ComputeDexRegisterMapSizeOf(const CodeInfoEncoding& encoding,
                                     uint32_t dex_register_map_offset_in_code_info,
                                     uint16_t number_of_dex_registers) const {
    // Offset where the actual mapping data starts within art::DexRegisterMap.
    size_t location_mapping_data_offset_in_dex_register_map =
        DexRegisterMap::GetLocationMappingDataOffset(number_of_dex_registers);
    // Create a temporary art::DexRegisterMap to be able to call
    // art::DexRegisterMap::GetNumberOfLiveDexRegisters and
    DexRegisterMap dex_register_map_without_locations(
        MemoryRegion(region_.Subregion(dex_register_map_offset_in_code_info,
                                       location_mapping_data_offset_in_dex_register_map)));
    size_t number_of_live_dex_registers =
        dex_register_map_without_locations.GetNumberOfLiveDexRegisters(number_of_dex_registers);
    size_t location_mapping_data_size_in_bits =
        DexRegisterMap::SingleEntrySizeInBits(GetNumberOfLocationCatalogEntries(encoding))
        * number_of_live_dex_registers;
    size_t location_mapping_data_size_in_bytes =
        RoundUp(location_mapping_data_size_in_bits, kBitsPerByte) / kBitsPerByte;
    size_t dex_register_map_size =
        location_mapping_data_offset_in_dex_register_map + location_mapping_data_size_in_bytes;
    return dex_register_map_size;
  }

  // Compute the size of a Dex register location catalog starting at offset `origin`
  // in `region_` and containing `number_of_dex_locations` entries.
  size_t ComputeDexRegisterLocationCatalogSize(uint32_t origin,
                                               uint32_t number_of_dex_locations) const {
    // TODO: Ideally, we would like to use art::DexRegisterLocationCatalog::Size or
    // art::DexRegisterLocationCatalog::FindLocationOffset, but the
    // DexRegisterLocationCatalog is not yet built.  Try to factor common code.
    size_t offset = origin + DexRegisterLocationCatalog::kFixedSize;

    // Skip the first `number_of_dex_locations - 1` entries.
    for (uint16_t i = 0; i < number_of_dex_locations; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a large location.
      DexRegisterLocationCatalog::ShortLocation first_byte =
          region_.LoadUnaligned<DexRegisterLocationCatalog::ShortLocation>(offset);
      DexRegisterLocation::Kind kind =
          DexRegisterLocationCatalog::ExtractKindFromShortLocation(first_byte);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += DexRegisterLocationCatalog::SingleShortEntrySize();
      } else {
        // Large location.  Skip the 5 next bytes.
        offset += DexRegisterLocationCatalog::SingleLargeEntrySize();
      }
    }
    size_t size = offset - origin;
    return size;
  }

  MemoryRegion region_;
  friend class StackMapStream;
};

#undef ELEMENT_BYTE_OFFSET_AFTER
#undef ELEMENT_BIT_OFFSET_AFTER

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
