/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cstring>

#include <profilo/writer/TimestampTruncatingVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

// Multiplication of two 64-bit numbers, keeping only the top 64 bits. This
// is necessary for the reciprocal multiplication optimization (see below).
// This could be simplified with __uint128_t, but unfortunately we don't have
// that type. If somehow/sometime it ever becomes available, we can get rid
// of this function and perform the multiplication directly.
uint64_t mulhi(uint64_t a, uint64_t b) {
  uint64_t a_lo = (uint32_t)a;
  uint64_t a_hi = a >> 32;
  uint64_t b_lo = (uint32_t)b;
  uint64_t b_hi = b >> 32;

  uint64_t a_x_b_hi = a_hi * b_hi;
  uint64_t a_x_b_mid = a_hi * b_lo;
  uint64_t b_x_a_mid = b_hi * a_lo;
  uint64_t a_x_b_lo = a_lo * b_lo;

  uint64_t carry_bit = ((uint64_t)(uint32_t)a_x_b_mid +
                        (uint64_t)(uint32_t)b_x_a_mid + (a_x_b_lo >> 32)) >>
      32;

  uint64_t multhi =
      a_x_b_hi + (a_x_b_mid >> 32) + (b_x_a_mid >> 32) + carry_bit;

  return multhi;
}

// Optimization to divide by 1000.
// See https://homepage.divms.uiowa.edu/~jones/bcd/divide.html
// In short, the insight is that it's faster to multiply by the reciprocal
// of a number than divide by it. In this case, the reciprocal of 1000
// is 0.001, which in fixed point notation is 0x4189374bc6a7f4 (with a
// 64 bit shift).
inline uint64_t div_1000(uint64_t num) {
  static constexpr uint64_t divisor = 0x4189374bc6a7f4;
  return mulhi(num, divisor);
}

TimestampTruncatingVisitor::TimestampTruncatingVisitor(
    EntryVisitor& delegate,
    size_t precision)
    : delegate_(delegate) {
  assert(precision == 6);
}

template <class T>
T TimestampTruncatingVisitor::truncateTimestamp(const T& entry) {
  T copied(entry);
  // This 500 comes from the denominator (1000) divided by 2. The math here is
  // (a + b/2) / b = (2a + b)/2b = a/b + 1/2 = round(a/b).
  // The denominator is always 1000 because that's what we use to truncate
  // ns timestamps into us.
  copied.timestamp = div_1000(copied.timestamp + 500);
  return copied;
}

void TimestampTruncatingVisitor::visit(const StandardEntry& entry) {
  auto std_entry = entry;
  delegate_.visit(truncateTimestamp(std_entry));
}

void TimestampTruncatingVisitor::visit(const FramesEntry& entry) {
  auto frames_entry = entry;
  delegate_.visit(truncateTimestamp(frames_entry));
}

void TimestampTruncatingVisitor::visit(const BytesEntry& entry) {
  delegate_.visit(entry);
}

} // namespace writer
} // namespace profilo
} // namespace facebook
