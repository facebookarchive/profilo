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

#ifndef ART_RUNTIME_BASE_BIT_VECTOR_H_
#define ART_RUNTIME_BASE_BIT_VECTOR_H_

#include <stdint.h>
#include <stddef.h>

#include "allocator.h"
#include "base/logging.h"
#include "utils.h"

namespace art {

/*
 * Expanding bitmap, used for tracking resources.  Bits are numbered starting
 * from zero.  All operations on a BitVector are unsynchronized.
 */
class BitVector {
  public:
    class IndexContainer;

    /**
     * @brief Convenient iterator across the indexes of the BitVector's set bits.
     *
     * @details IndexIterator is a Forward iterator (C++11: 24.2.5) from the lowest
     * to the highest index of the BitVector's set bits. Instances can be retrieved
     * only through BitVector::Indexes() which returns an IndexContainer wrapper
     * object with begin() and end() suitable for range-based loops:
     *   for (uint32_t idx : bit_vector.Indexes()) {
     *     // Use idx.
     *   }
     */
    class IndexIterator
        : std::iterator<std::forward_iterator_tag, uint32_t, ptrdiff_t, void, uint32_t> {
      public:
        bool operator==(const IndexIterator& other) const {
          DCHECK(bit_storage_ == other.bit_storage_);
          DCHECK_EQ(storage_size_, other.storage_size_);
          return bit_index_ == other.bit_index_;
        }

        bool operator!=(const IndexIterator& other) const {
          return !(*this == other);
        }

        int operator*() const {
          DCHECK_LT(bit_index_, BitSize());
          return bit_index_;
        }

        IndexIterator& operator++() {
          DCHECK_LT(bit_index_, BitSize());
          bit_index_ = FindIndex(bit_index_ + 1u);
          return *this;
        }

        IndexIterator operator++(int) {
          IndexIterator result(*this);
          ++*this;
          return result;
        }

        // Helper function to check for end without comparing with bit_vector.Indexes().end().
        bool Done() const {
          return bit_index_ == BitSize();
        }

      private:
        struct begin_tag { };
        struct end_tag { };

        IndexIterator(const BitVector* bit_vector, begin_tag)
          : bit_storage_(bit_vector->GetRawStorage()),
            storage_size_(bit_vector->storage_size_),
            bit_index_(FindIndex(0u)) { }

        IndexIterator(const BitVector* bit_vector, end_tag)
          : bit_storage_(bit_vector->GetRawStorage()),
            storage_size_(bit_vector->storage_size_),
            bit_index_(BitSize()) { }

        uint32_t BitSize() const {
          return storage_size_ * kWordBits;
        }

        uint32_t FindIndex(uint32_t start_index) const {
          DCHECK_LE(start_index, BitSize());
          uint32_t word_index = start_index / kWordBits;
          if (UNLIKELY(word_index == storage_size_)) {
            return start_index;
          }
          uint32_t word = bit_storage_[word_index];
          // Mask out any bits in the first word we've already considered.
          word &= static_cast<uint32_t>(-1) << (start_index & 0x1f);
          while (word == 0u) {
            ++word_index;
            if (UNLIKELY(word_index == storage_size_)) {
              return BitSize();
            }
            word = bit_storage_[word_index];
          }
          return word_index * 32u + CTZ(word);
        }

        const uint32_t* const bit_storage_;
        const uint32_t storage_size_;  // Size of vector in words.
        uint32_t bit_index_;           // Current index (size in bits).

        friend class BitVector::IndexContainer;
    };

    /**
     * @brief BitVector wrapper class for iteration across indexes of set bits.
     */
    class IndexContainer {
     public:
      explicit IndexContainer(const BitVector* bit_vector) : bit_vector_(bit_vector) { }

      IndexIterator begin() const {
        return IndexIterator(bit_vector_, IndexIterator::begin_tag());
      }

      IndexIterator end() const {
        return IndexIterator(bit_vector_, IndexIterator::end_tag());
      }

     private:
      const BitVector* const bit_vector_;
    };

    BitVector(uint32_t start_bits,
              bool expandable,
              Allocator* allocator,
              uint32_t storage_size = 0,
              uint32_t* storage = nullptr);

    virtual ~BitVector();

    void SetBit(uint32_t num);
    void ClearBit(uint32_t num);
    bool IsBitSet(uint32_t num) const;
    void ClearAllBits();
    void SetInitialBits(uint32_t num_bits);

    void Copy(const BitVector* src);
    void Intersect(const BitVector* src2);
    bool Union(const BitVector* src);

    // Set bits of union_with that are not in not_in.
    bool UnionIfNotIn(const BitVector* union_with, const BitVector* not_in);

    void Subtract(const BitVector* src);
    // Are we equal to another bit vector?  Note: expandability attributes must also match.
    bool Equal(const BitVector* src) {
      return (storage_size_ == src->GetStorageSize()) &&
        (expandable_ == src->IsExpandable()) &&
        (memcmp(storage_, src->GetRawStorage(), storage_size_ * sizeof(uint32_t)) == 0);
    }

    /**
     * @brief Are all the bits set the same?
     * @details expandability and size can differ as long as the same bits are set.
     */
    bool SameBitsSet(const BitVector *src);

    uint32_t NumSetBits() const;

    // Number of bits set in range [0, end).
    uint32_t NumSetBits(uint32_t end) const;

    IndexContainer Indexes() const {
      return IndexContainer(this);
    }

    uint32_t GetStorageSize() const { return storage_size_; }
    bool IsExpandable() const { return expandable_; }
    uint32_t GetRawStorageWord(size_t idx) const { return storage_[idx]; }
    uint32_t* GetRawStorage() { return storage_; }
    const uint32_t* GetRawStorage() const { return storage_; }
    size_t GetSizeOf() const { return storage_size_ * kWordBytes; }

    /**
     * @return the highest bit set, -1 if none are set
     */
    int GetHighestBitSet() const;

    // Is bit set in storage. (No range check.)
    static bool IsBitSet(const uint32_t* storage, uint32_t num);
    // Number of bits set in range [0, end) in storage. (No range check.)
    static uint32_t NumSetBits(const uint32_t* storage, uint32_t end);

    bool EnsureSizeAndClear(unsigned int num);

    void Dump(std::ostream& os, const char* prefix) const;

    /**
     * @brief last_entry is this the last entry for the dot dumping
     * @details if not, a "|" is appended to the dump.
     */
    void DumpDot(FILE* file, const char* prefix, bool last_entry = false) const;

    /**
     * @brief last_entry is this the last entry for the dot dumping
     * @details if not, a "|" is appended to the dump.
     */
    void DumpIndicesDot(FILE* file, const char* prefix, bool last_entry = false) const;

  protected:
    /**
     * @brief Dump the bitvector into buffer in a 00101..01 format.
     * @param buffer the ostringstream used to dump the bitvector into.
     */
    void DumpHelper(const char* prefix, std::ostringstream& buffer) const;

    /**
     * @brief Dump the bitvector in a 1 2 5 8 format, where the numbers are the bit set.
     * @param buffer the ostringstream used to dump the bitvector into.
     */
    void DumpIndicesHelper(const char* prefix, std::ostringstream& buffer) const;

    /**
     * @brief Wrapper to perform the bitvector dumping with the .dot format.
     * @param buffer the ostringstream used to dump the bitvector into.
     */
    void DumpDotHelper(bool last_entry, FILE* file, std::ostringstream& buffer) const;

  private:
    static constexpr uint32_t kWordBytes = sizeof(uint32_t);
    static constexpr uint32_t kWordBits = kWordBytes * 8;

    Allocator* const allocator_;
    const bool expandable_;         // expand bitmap if we run out?
    uint32_t   storage_size_;       // current size, in 32-bit words.
    uint32_t*  storage_;
};


}  // namespace art

#endif  // ART_RUNTIME_BASE_BIT_VECTOR_H_
