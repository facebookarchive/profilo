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

#ifndef ART_RUNTIME_GC_ACCOUNTING_BITMAP_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_BITMAP_INL_H_

#include "bitmap.h"

#include <memory>

#include "atomic.h"
#include "base/bit_utils.h"
#include "base/logging.h"

namespace art {
namespace gc {
namespace accounting {

inline bool Bitmap::AtomicTestAndSetBit(uintptr_t bit_index) {
  CheckValidBitIndex(bit_index);
  const size_t word_index = BitIndexToWordIndex(bit_index);
  const uintptr_t word_mask = BitIndexToMask(bit_index);
  auto* atomic_entry = reinterpret_cast<Atomic<uintptr_t>*>(&bitmap_begin_[word_index]);
  uintptr_t old_word;
  do {
    old_word = atomic_entry->LoadRelaxed();
    // Fast path: The bit is already set.
    if ((old_word & word_mask) != 0) {
      DCHECK(TestBit(bit_index));
      return true;
    }
  } while (!atomic_entry->CompareExchangeWeakSequentiallyConsistent(old_word,
                                                                    old_word | word_mask));
  DCHECK(TestBit(bit_index));
  return false;
}

inline bool Bitmap::TestBit(uintptr_t bit_index) const {
  CheckValidBitIndex(bit_index);
  return (bitmap_begin_[BitIndexToWordIndex(bit_index)] & BitIndexToMask(bit_index)) != 0;
}

template<typename Visitor>
inline void Bitmap::VisitSetBits(uintptr_t bit_start, uintptr_t bit_end, const Visitor& visitor)
    const {
  DCHECK_LE(bit_start, bit_end);
  CheckValidBitIndex(bit_start);
  const uintptr_t index_start = BitIndexToWordIndex(bit_start);
  const uintptr_t index_end = BitIndexToWordIndex(bit_end);
  if (bit_start != bit_end) {
    CheckValidBitIndex(bit_end - 1);
  }

  // Index(begin)  ...    Index(end)
  // [xxxxx???][........][????yyyy]
  //      ^                   ^
  //      |                   #---- Bit of visit_end
  //      #---- Bit of visit_begin
  //

  // Left edge.
  uintptr_t left_edge = bitmap_begin_[index_start];
  // Clear the lower bits that are not in range.
  left_edge &= ~((static_cast<uintptr_t>(1) << (bit_start % kBitsPerBitmapWord)) - 1);

  // Right edge. Either unique, or left_edge.
  uintptr_t right_edge;

  if (index_start < index_end) {
    // Left edge != right edge.

    // Traverse left edge.
    if (left_edge != 0) {
      const uintptr_t ptr_base = WordIndexToBitIndex(index_start);
      do {
        const size_t shift = CTZ(left_edge);
        visitor(ptr_base + shift);
        left_edge ^= static_cast<uintptr_t>(1) << shift;
      } while (left_edge != 0);
    }

    // Traverse the middle, full part.
    for (size_t i = index_start + 1; i < index_end; ++i) {
      uintptr_t w = bitmap_begin_[i];
      if (w != 0) {
        const uintptr_t ptr_base = WordIndexToBitIndex(i);
        do {
          const size_t shift = CTZ(w);
          visitor(ptr_base + shift);
          w ^= static_cast<uintptr_t>(1) << shift;
        } while (w != 0);
      }
    }

    // Right edge is unique.
    // But maybe we don't have anything to do: visit_end starts in a new word...
    if (bit_end == 0) {
      // Do not read memory, as it could be after the end of the bitmap.
      right_edge = 0;
    } else {
      right_edge = bitmap_begin_[index_end];
    }
  } else {
    right_edge = left_edge;
  }

  // Right edge handling.
  right_edge &= ((static_cast<uintptr_t>(1) << (bit_end % kBitsPerBitmapWord)) - 1);
  if (right_edge != 0) {
    const uintptr_t ptr_base = WordIndexToBitIndex(index_end);
    do {
      const size_t shift = CTZ(right_edge);
      visitor(ptr_base + shift);
      right_edge ^= (static_cast<uintptr_t>(1)) << shift;
    } while (right_edge != 0);
  }
}

template<bool kSetBit>
inline bool Bitmap::ModifyBit(uintptr_t bit_index) {
  CheckValidBitIndex(bit_index);
  const size_t word_index = BitIndexToWordIndex(bit_index);
  const uintptr_t word_mask = BitIndexToMask(bit_index);
  uintptr_t* address = &bitmap_begin_[word_index];
  uintptr_t old_word = *address;
  if (kSetBit) {
    *address = old_word | word_mask;
  } else {
    *address = old_word & ~word_mask;
  }
  DCHECK_EQ(TestBit(bit_index), kSetBit);
  return (old_word & word_mask) != 0;
}

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_BITMAP_INL_H_
