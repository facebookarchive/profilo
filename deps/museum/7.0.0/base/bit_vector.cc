/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "bit_vector.h"

#include <limits>
#include <sstream>

#include "allocator.h"
#include "bit_vector-inl.h"

namespace art {

uint32_t BitVector::NumSetBits(const uint32_t* storage, uint32_t end) {
  uint32_t word_end = WordIndex(end);
  uint32_t partial_word_bits = end & 0x1f;

  uint32_t count = 0u;
  for (uint32_t word = 0u; word < word_end; word++) {
    count += POPCOUNT(storage[word]);
  }
  if (partial_word_bits != 0u) {
    count += POPCOUNT(storage[word_end] & ~(0xffffffffu << partial_word_bits));
  }
  return count;
}

}  // namespace art
