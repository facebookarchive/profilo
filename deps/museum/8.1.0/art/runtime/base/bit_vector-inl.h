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

#ifndef ART_RUNTIME_BASE_BIT_VECTOR_INL_H_
#define ART_RUNTIME_BASE_BIT_VECTOR_INL_H_

#include "base/bit_utils.h"
#include "bit_vector.h"
#include "logging.h"

namespace art {

inline bool BitVector::IndexIterator::operator==(const IndexIterator& other) const {
  DCHECK(bit_storage_ == other.bit_storage_);
  DCHECK_EQ(storage_size_, other.storage_size_);
  return bit_index_ == other.bit_index_;
}

inline uint32_t BitVector::IndexIterator::operator*() const {
  DCHECK_LT(bit_index_, BitSize());
  return bit_index_;
}

inline BitVector::IndexIterator& BitVector::IndexIterator::operator++() {
  DCHECK_LT(bit_index_, BitSize());
  bit_index_ = FindIndex(bit_index_ + 1u);
  return *this;
}

inline BitVector::IndexIterator BitVector::IndexIterator::operator++(int) {
  IndexIterator result(*this);
  ++*this;
  return result;
}

inline uint32_t BitVector::IndexIterator::FindIndex(uint32_t start_index) const {
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

inline void BitVector::ClearAllBits() {
  memset(storage_, 0, storage_size_ * kWordBytes);
}

inline bool BitVector::Equal(const BitVector* src) const {
  return (storage_size_ == src->GetStorageSize()) &&
    (expandable_ == src->IsExpandable()) &&
    (memcmp(storage_, src->GetRawStorage(), storage_size_ * sizeof(uint32_t)) == 0);
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_BIT_VECTOR_INL_H_
