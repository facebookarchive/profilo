/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_INL_H_

#include "space_bitmap.h"

#include <memory>

#include "atomic.h"
#include "base/logging.h"
#include "utils.h"

namespace art {
namespace gc {
namespace accounting {

template<size_t kAlignment>
inline bool SpaceBitmap<kAlignment>::AtomicTestAndSet(const mirror::Object* obj) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
  DCHECK_GE(addr, heap_begin_);
  const uintptr_t offset = addr - heap_begin_;
  const size_t index = OffsetToIndex(offset);
  const uword mask = OffsetToMask(offset);
  Atomic<uword>* atomic_entry = reinterpret_cast<Atomic<uword>*>(&bitmap_begin_[index]);
  DCHECK_LT(index, bitmap_size_ / kWordSize) << " bitmap_size_ = " << bitmap_size_;
  uword old_word;
  do {
    old_word = atomic_entry->LoadRelaxed();
    // Fast path: The bit is already set.
    if ((old_word & mask) != 0) {
      DCHECK(Test(obj));
      return true;
    }
  } while (!atomic_entry->CompareExchangeWeakSequentiallyConsistent(old_word, old_word | mask));
  DCHECK(Test(obj));
  return false;
}

template<size_t kAlignment>
inline bool SpaceBitmap<kAlignment>::Test(const mirror::Object* obj) const {
  uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
  DCHECK(HasAddress(obj)) << obj;
  DCHECK(bitmap_begin_ != NULL);
  DCHECK_GE(addr, heap_begin_);
  const uintptr_t offset = addr - heap_begin_;
  return (bitmap_begin_[OffsetToIndex(offset)] & OffsetToMask(offset)) != 0;
}

template<size_t kAlignment> template<typename Visitor>
inline void SpaceBitmap<kAlignment>::VisitMarkedRange(uintptr_t visit_begin, uintptr_t visit_end,
                                                      const Visitor& visitor) const {
  DCHECK_LE(visit_begin, visit_end);
#if 0
  for (uintptr_t i = visit_begin; i < visit_end; i += kAlignment) {
    mirror::Object* obj = reinterpret_cast<mirror::Object*>(i);
    if (Test(obj)) {
      visitor(obj);
    }
  }
#else
  DCHECK_LE(heap_begin_, visit_begin);
  DCHECK_LE(visit_end, HeapLimit());

  const uintptr_t offset_start = visit_begin - heap_begin_;
  const uintptr_t offset_end = visit_end - heap_begin_;

  const uintptr_t index_start = OffsetToIndex(offset_start);
  const uintptr_t index_end = OffsetToIndex(offset_end);

  const size_t bit_start = (offset_start / kAlignment) % kBitsPerWord;
  const size_t bit_end = (offset_end / kAlignment) % kBitsPerWord;

  // Index(begin)  ...    Index(end)
  // [xxxxx???][........][????yyyy]
  //      ^                   ^
  //      |                   #---- Bit of visit_end
  //      #---- Bit of visit_begin
  //

  // Left edge.
  uword left_edge = bitmap_begin_[index_start];
  // Mark of lower bits that are not in range.
  left_edge &= ~((static_cast<uword>(1) << bit_start) - 1);

  // Right edge. Either unique, or left_edge.
  uword right_edge;

  if (index_start < index_end) {
    // Left edge != right edge.

    // Traverse left edge.
    if (left_edge != 0) {
      const uintptr_t ptr_base = IndexToOffset(index_start) + heap_begin_;
      do {
        const size_t shift = CTZ(left_edge);
        mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
        visitor(obj);
        left_edge ^= (static_cast<uword>(1)) << shift;
      } while (left_edge != 0);
    }

    // Traverse the middle, full part.
    for (size_t i = index_start + 1; i < index_end; ++i) {
      uword w = bitmap_begin_[i];
      if (w != 0) {
        const uintptr_t ptr_base = IndexToOffset(i) + heap_begin_;
        do {
          const size_t shift = CTZ(w);
          mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
          visitor(obj);
          w ^= (static_cast<uword>(1)) << shift;
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
    // Right edge = left edge.
    right_edge = left_edge;
  }

  // Right edge handling.
  right_edge &= ((static_cast<uword>(1) << bit_end) - 1);
  if (right_edge != 0) {
    const uintptr_t ptr_base = IndexToOffset(index_end) + heap_begin_;
    do {
      const size_t shift = CTZ(right_edge);
      mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
      visitor(obj);
      right_edge ^= (static_cast<uword>(1)) << shift;
    } while (right_edge != 0);
  }
#endif
}

template<size_t kAlignment> template<bool kSetBit>
inline bool SpaceBitmap<kAlignment>::Modify(const mirror::Object* obj) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
  DCHECK_GE(addr, heap_begin_);
  const uintptr_t offset = addr - heap_begin_;
  const size_t index = OffsetToIndex(offset);
  const uword mask = OffsetToMask(offset);
  DCHECK_LT(index, bitmap_size_ / kWordSize) << " bitmap_size_ = " << bitmap_size_;
  uword* address = &bitmap_begin_[index];
  uword old_word = *address;
  if (kSetBit) {
    *address = old_word | mask;
  } else {
    *address = old_word & ~mask;
  }
  DCHECK_EQ(Test(obj), kSetBit);
  return (old_word & mask) != 0;
}

template<size_t kAlignment>
inline std::ostream& operator << (std::ostream& stream, const SpaceBitmap<kAlignment>& bitmap) {
  return stream
    << bitmap.GetName() << "["
    << "begin=" << reinterpret_cast<const void*>(bitmap.HeapBegin())
    << ",end=" << reinterpret_cast<const void*>(bitmap.HeapLimit())
    << "]";
}

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_INL_H_
