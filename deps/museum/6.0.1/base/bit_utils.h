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

#ifndef ART_RUNTIME_BASE_BIT_UTILS_H_
#define ART_RUNTIME_BASE_BIT_UTILS_H_

#include <iterator>
#include <limits>
#include <type_traits>

#include "base/logging.h"
#include "base/iteration_range.h"

namespace art {

template<typename T>
static constexpr int CLZ(T x) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  // TODO: assert unsigned. There is currently many uses with signed values.
  static_assert(sizeof(T) <= sizeof(long long),  // NOLINT [runtime/int] [4]
                "T too large, must be smaller than long long");
  return (sizeof(T) == sizeof(uint32_t))
      ? __builtin_clz(x)  // TODO: __builtin_clz[ll] has undefined behavior for x=0
      : __builtin_clzll(x);
}

template<typename T>
static constexpr int CTZ(T x) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  // TODO: assert unsigned. There is currently many uses with signed values.
  return (sizeof(T) == sizeof(uint32_t))
      ? __builtin_ctz(x)
      : __builtin_ctzll(x);
}

template<typename T>
static constexpr int POPCOUNT(T x) {
  return (sizeof(T) == sizeof(uint32_t))
      ? __builtin_popcount(x)
      : __builtin_popcountll(x);
}

// Find the bit position of the most significant bit (0-based), or -1 if there were no bits set.
template <typename T>
static constexpr ssize_t MostSignificantBit(T value) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");
  static_assert(std::numeric_limits<T>::radix == 2, "Unexpected radix!");
  return (value == 0) ? -1 : std::numeric_limits<T>::digits - 1 - CLZ(value);
}

// Find the bit position of the least significant bit (0-based), or -1 if there were no bits set.
template <typename T>
static constexpr ssize_t LeastSignificantBit(T value) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");
  return (value == 0) ? -1 : CTZ(value);
}

// How many bits (minimally) does it take to store the constant 'value'? i.e. 1 for 1, 3 for 5, etc.
template <typename T>
static constexpr size_t MinimumBitsToStore(T value) {
  return static_cast<size_t>(MostSignificantBit(value) + 1);
}

template <typename T>
static constexpr inline T RoundUpToPowerOfTwo(T x) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");
  // NOTE: Undefined if x > (1 << (std::numeric_limits<T>::digits - 1)).
  return (x < 2u) ? x : static_cast<T>(1u) << (std::numeric_limits<T>::digits - CLZ(x - 1u));
}

template<typename T>
static constexpr bool IsPowerOfTwo(T x) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  // TODO: assert unsigned. There is currently many uses with signed values.
  return (x & (x - 1)) == 0;
}

template<typename T>
static inline int WhichPowerOf2(T x) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  // TODO: assert unsigned. There is currently many uses with signed values.
  DCHECK((x != 0) && IsPowerOfTwo(x));
  return CTZ(x);
}

// For rounding integers.
// NOTE: In the absence of std::omit_from_type_deduction<T> or std::identity<T>, use std::decay<T>.
template<typename T>
static constexpr T RoundDown(T x, typename std::decay<T>::type n) WARN_UNUSED;

template<typename T>
static constexpr T RoundDown(T x, typename std::decay<T>::type n) {
  return
      DCHECK_CONSTEXPR(IsPowerOfTwo(n), , T(0))
      (x & -n);
}

template<typename T>
static constexpr T RoundUp(T x, typename std::remove_reference<T>::type n) WARN_UNUSED;

template<typename T>
static constexpr T RoundUp(T x, typename std::remove_reference<T>::type n) {
  return RoundDown(x + n - 1, n);
}

// For aligning pointers.
template<typename T>
static inline T* AlignDown(T* x, uintptr_t n) WARN_UNUSED;

template<typename T>
static inline T* AlignDown(T* x, uintptr_t n) {
  return reinterpret_cast<T*>(RoundDown(reinterpret_cast<uintptr_t>(x), n));
}

template<typename T>
static inline T* AlignUp(T* x, uintptr_t n) WARN_UNUSED;

template<typename T>
static inline T* AlignUp(T* x, uintptr_t n) {
  return reinterpret_cast<T*>(RoundUp(reinterpret_cast<uintptr_t>(x), n));
}

template<int n, typename T>
static inline bool IsAligned(T x) {
  static_assert((n & (n - 1)) == 0, "n is not a power of two");
  return (x & (n - 1)) == 0;
}

template<int n, typename T>
static inline bool IsAligned(T* x) {
  return IsAligned<n>(reinterpret_cast<const uintptr_t>(x));
}

template<typename T>
static inline bool IsAlignedParam(T x, int n) {
  return (x & (n - 1)) == 0;
}

#define CHECK_ALIGNED(value, alignment) \
  CHECK(::art::IsAligned<alignment>(value)) << reinterpret_cast<const void*>(value)

#define DCHECK_ALIGNED(value, alignment) \
  DCHECK(::art::IsAligned<alignment>(value)) << reinterpret_cast<const void*>(value)

#define DCHECK_ALIGNED_PARAM(value, alignment) \
  DCHECK(::art::IsAlignedParam(value, alignment)) << reinterpret_cast<const void*>(value)

// Like sizeof, but count how many bits a type takes. Pass type explicitly.
template <typename T>
static constexpr size_t BitSizeOf() {
  static_assert(std::is_integral<T>::value, "T must be integral");
  typedef typename std::make_unsigned<T>::type unsigned_type;
  static_assert(sizeof(T) == sizeof(unsigned_type), "Unexpected type size mismatch!");
  static_assert(std::numeric_limits<unsigned_type>::radix == 2, "Unexpected radix!");
  return std::numeric_limits<unsigned_type>::digits;
}

// Like sizeof, but count how many bits a type takes. Infers type from parameter.
template <typename T>
static constexpr size_t BitSizeOf(T /*x*/) {
  return BitSizeOf<T>();
}

static inline uint16_t Low16Bits(uint32_t value) {
  return static_cast<uint16_t>(value);
}

static inline uint16_t High16Bits(uint32_t value) {
  return static_cast<uint16_t>(value >> 16);
}

static inline uint32_t Low32Bits(uint64_t value) {
  return static_cast<uint32_t>(value);
}

static inline uint32_t High32Bits(uint64_t value) {
  return static_cast<uint32_t>(value >> 32);
}

// Check whether an N-bit two's-complement representation can hold value.
template <typename T>
static inline bool IsInt(size_t N, T value) {
  if (N == BitSizeOf<T>()) {
    return true;
  } else {
    CHECK_LT(0u, N);
    CHECK_LT(N, BitSizeOf<T>());
    T limit = static_cast<T>(1) << (N - 1u);
    return (-limit <= value) && (value < limit);
  }
}

template <typename T>
static constexpr T GetIntLimit(size_t bits) {
  return
      DCHECK_CONSTEXPR(bits > 0, "bits cannot be zero", 0)
      DCHECK_CONSTEXPR(bits < BitSizeOf<T>(), "kBits must be < max.", 0)
      static_cast<T>(1) << (bits - 1);
}

template <size_t kBits, typename T>
static constexpr bool IsInt(T value) {
  static_assert(kBits > 0, "kBits cannot be zero.");
  static_assert(kBits <= BitSizeOf<T>(), "kBits must be <= max.");
  static_assert(std::is_signed<T>::value, "Needs a signed type.");
  // Corner case for "use all bits." Can't use the limits, as they would overflow, but it is
  // trivially true.
  return (kBits == BitSizeOf<T>()) ?
      true :
      (-GetIntLimit<T>(kBits) <= value) && (value < GetIntLimit<T>(kBits));
}

template <size_t kBits, typename T>
static constexpr bool IsUint(T value) {
  static_assert(kBits > 0, "kBits cannot be zero.");
  static_assert(kBits <= BitSizeOf<T>(), "kBits must be <= max.");
  static_assert(std::is_integral<T>::value, "Needs an integral type.");
  // Corner case for "use all bits." Can't use the limits, as they would overflow, but it is
  // trivially true.
  // NOTE: To avoid triggering assertion in GetIntLimit(kBits+1) if kBits+1==BitSizeOf<T>(),
  // use GetIntLimit(kBits)*2u. The unsigned arithmetic works well for us if it overflows.
  return (0 <= value) &&
      (kBits == BitSizeOf<T>() ||
          (static_cast<typename std::make_unsigned<T>::type>(value) <=
               GetIntLimit<typename std::make_unsigned<T>::type>(kBits) * 2u - 1u));
}

template <size_t kBits, typename T>
static constexpr bool IsAbsoluteUint(T value) {
  static_assert(kBits <= BitSizeOf<T>(), "kBits must be <= max.");
  static_assert(std::is_integral<T>::value, "Needs an integral type.");
  typedef typename std::make_unsigned<T>::type unsigned_type;
  return (kBits == BitSizeOf<T>())
      ? true
      : IsUint<kBits>(value < 0
                      ? static_cast<unsigned_type>(-1 - value) + 1u  // Avoid overflow.
                      : static_cast<unsigned_type>(value));
}

// Using the Curiously Recurring Template Pattern to implement everything shared
// by LowToHighBitIterator and HighToLowBitIterator, i.e. everything but operator*().
template <typename T, typename Iter>
class BitIteratorBase
    : public std::iterator<std::forward_iterator_tag, uint32_t, ptrdiff_t, void, void> {
  static_assert(std::is_integral<T>::value, "T must be integral");
  static_assert(std::is_unsigned<T>::value, "T must be unsigned");

  static_assert(sizeof(T) == sizeof(uint32_t) || sizeof(T) == sizeof(uint64_t), "Unsupported size");

 public:
  BitIteratorBase() : bits_(0u) { }
  explicit BitIteratorBase(T bits) : bits_(bits) { }

  Iter& operator++() {
    DCHECK_NE(bits_, 0u);
    uint32_t bit = *static_cast<Iter&>(*this);
    bits_ &= ~(static_cast<T>(1u) << bit);
    return static_cast<Iter&>(*this);
  }

  Iter& operator++(int) {
    Iter tmp(static_cast<Iter&>(*this));
    ++*this;
    return tmp;
  }

 protected:
  T bits_;

  template <typename U, typename I>
  friend bool operator==(const BitIteratorBase<U, I>& lhs, const BitIteratorBase<U, I>& rhs);
};

template <typename T, typename Iter>
bool operator==(const BitIteratorBase<T, Iter>& lhs, const BitIteratorBase<T, Iter>& rhs) {
  return lhs.bits_ == rhs.bits_;
}

template <typename T, typename Iter>
bool operator!=(const BitIteratorBase<T, Iter>& lhs, const BitIteratorBase<T, Iter>& rhs) {
  return !(lhs == rhs);
}

template <typename T>
class LowToHighBitIterator : public BitIteratorBase<T, LowToHighBitIterator<T>> {
 public:
  using BitIteratorBase<T, LowToHighBitIterator<T>>::BitIteratorBase;

  uint32_t operator*() const {
    DCHECK_NE(this->bits_, 0u);
    return CTZ(this->bits_);
  }
};

template <typename T>
class HighToLowBitIterator : public BitIteratorBase<T, HighToLowBitIterator<T>> {
 public:
  using BitIteratorBase<T, HighToLowBitIterator<T>>::BitIteratorBase;

  uint32_t operator*() const {
    DCHECK_NE(this->bits_, 0u);
    static_assert(std::numeric_limits<T>::radix == 2, "Unexpected radix!");
    return std::numeric_limits<T>::digits - 1u - CLZ(this->bits_);
  }
};

template <typename T>
IterationRange<LowToHighBitIterator<T>> LowToHighBits(T bits) {
  return IterationRange<LowToHighBitIterator<T>>(
      LowToHighBitIterator<T>(bits), LowToHighBitIterator<T>());
}

template <typename T>
IterationRange<HighToLowBitIterator<T>> HighToLowBits(T bits) {
  return IterationRange<HighToLowBitIterator<T>>(
      HighToLowBitIterator<T>(bits), HighToLowBitIterator<T>());
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_BIT_UTILS_H_
