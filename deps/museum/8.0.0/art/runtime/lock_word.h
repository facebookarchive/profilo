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

#ifndef ART_RUNTIME_LOCK_WORD_H_
#define ART_RUNTIME_LOCK_WORD_H_

#include <iosfwd>
#include <stdint.h>

#include "base/bit_utils.h"
#include "base/logging.h"
#include "read_barrier.h"

namespace art {
namespace mirror {
  class Object;
}  // namespace mirror

class Monitor;

/* The lock value itself as stored in mirror::Object::monitor_.  The two most significant bits of
 * the state. The four possible states are fat locked, thin/unlocked, hash code, and forwarding
 * address. When the lock word is in the "thin" state and its bits are formatted as follows:
 *
 *  |33|2|2|222222221111|1111110000000000|
 *  |10|9|8|765432109876|5432109876543210|
 *  |00|m|r| lock count |thread id owner |
 *
 * When the lock word is in the "fat" state and its bits are formatted as follows:
 *
 *  |33|2|2|2222222211111111110000000000|
 *  |10|9|8|7654321098765432109876543210|
 *  |01|m|r| MonitorId                  |
 *
 * When the lock word is in hash state and its bits are formatted as follows:
 *
 *  |33|2|2|2222222211111111110000000000|
 *  |10|9|8|7654321098765432109876543210|
 *  |10|m|r| HashCode                   |
 *
 * When the lock word is in forwarding address state and its bits are formatted as follows:
 *
 *  |33|2|22222222211111111110000000000|
 *  |10|9|87654321098765432109876543210|
 *  |11|0| ForwardingAddress           |
 *
 * The `r` bit stores the read barrier state.
 * The `m` bit stores the mark state.
 */
class LockWord {
 public:
  enum SizeShiftsAndMasks : uint32_t {  // private marker to avoid generate-operator-out.py from processing.
    // Number of bits to encode the state, currently just fat or thin/unlocked or hash code.
    kStateSize = 2,
    kReadBarrierStateSize = 1,
    kMarkBitStateSize = 1,
    // Number of bits to encode the thin lock owner.
    kThinLockOwnerSize = 16,
    // Remaining bits are the recursive lock count.
    kThinLockCountSize = 32 - kThinLockOwnerSize - kStateSize - kReadBarrierStateSize -
        kMarkBitStateSize,
    // Thin lock bits. Owner in lowest bits.

    kThinLockOwnerShift = 0,
    kThinLockOwnerMask = (1 << kThinLockOwnerSize) - 1,
    kThinLockMaxOwner = kThinLockOwnerMask,
    // Count in higher bits.
    kThinLockCountShift = kThinLockOwnerSize + kThinLockOwnerShift,
    kThinLockCountMask = (1 << kThinLockCountSize) - 1,
    kThinLockMaxCount = kThinLockCountMask,
    kThinLockCountOne = 1 << kThinLockCountShift,  // == 65536 (0x10000)

    // State in the highest bits.
    kStateShift = kReadBarrierStateSize + kThinLockCountSize + kThinLockCountShift +
        kMarkBitStateSize,
    kStateMask = (1 << kStateSize) - 1,
    kStateMaskShifted = kStateMask << kStateShift,
    kStateThinOrUnlocked = 0,
    kStateFat = 1,
    kStateHash = 2,
    kStateForwardingAddress = 3,
    kStateForwardingAddressShifted = kStateForwardingAddress << kStateShift,
    kStateForwardingAddressOverflow = (1 + kStateMask - kStateForwardingAddress) << kStateShift,

    // Read barrier bit.
    kReadBarrierStateShift = kThinLockCountSize + kThinLockCountShift,
    kReadBarrierStateMask = (1 << kReadBarrierStateSize) - 1,
    kReadBarrierStateMaskShifted = kReadBarrierStateMask << kReadBarrierStateShift,
    kReadBarrierStateMaskShiftedToggled = ~kReadBarrierStateMaskShifted,

    // Mark bit.
    kMarkBitStateShift = kReadBarrierStateSize + kReadBarrierStateShift,
    kMarkBitStateMask = (1 << kMarkBitStateSize) - 1,
    kMarkBitStateMaskShifted = kMarkBitStateMask << kMarkBitStateShift,
    kMarkBitStateMaskShiftedToggled = ~kMarkBitStateMaskShifted,

    // GC state is mark bit and read barrier state.
    kGCStateSize = kReadBarrierStateSize + kMarkBitStateSize,
    kGCStateShift = kReadBarrierStateShift,
    kGCStateMaskShifted = kReadBarrierStateMaskShifted | kMarkBitStateMaskShifted,
    kGCStateMaskShiftedToggled = ~kGCStateMaskShifted,

    // When the state is kHashCode, the non-state bits hold the hashcode.
    // Note Object.hashCode() has the hash code layout hardcoded.
    kHashShift = 0,
    kHashSize = 32 - kStateSize - kReadBarrierStateSize - kMarkBitStateSize,
    kHashMask = (1 << kHashSize) - 1,
    kMaxHash = kHashMask,

    // Forwarding address shift.
    kForwardingAddressShift = kObjectAlignmentShift,

    kMonitorIdShift = kHashShift,
    kMonitorIdSize = kHashSize,
    kMonitorIdMask = kHashMask,
    kMonitorIdAlignmentShift = 32 - kMonitorIdSize,
    kMonitorIdAlignment = 1 << kMonitorIdAlignmentShift,
    kMaxMonitorId = kMaxHash
  };

  static LockWord FromThinLockId(uint32_t thread_id, uint32_t count, uint32_t gc_state) {
    CHECK_LE(thread_id, static_cast<uint32_t>(kThinLockMaxOwner));
    CHECK_LE(count, static_cast<uint32_t>(kThinLockMaxCount));
    // DCHECK_EQ(gc_bits & kGCStateMaskToggled, 0U);
    return LockWord((thread_id << kThinLockOwnerShift) |
                    (count << kThinLockCountShift) |
                    (gc_state << kGCStateShift) |
                    (kStateThinOrUnlocked << kStateShift));
  }

  static LockWord FromForwardingAddress(size_t target) {
    DCHECK_ALIGNED(target, (1 << kStateSize));
    return LockWord((target >> kForwardingAddressShift) | kStateForwardingAddressShifted);
  }

  static LockWord FromHashCode(uint32_t hash_code, uint32_t gc_state) {
    CHECK_LE(hash_code, static_cast<uint32_t>(kMaxHash));
    // DCHECK_EQ(gc_bits & kGCStateMaskToggled, 0U);
    return LockWord((hash_code << kHashShift) |
                    (gc_state << kGCStateShift) |
                    (kStateHash << kStateShift));
  }

  static LockWord FromDefault(uint32_t gc_state) {
    return LockWord(gc_state << kGCStateShift);
  }

  static bool IsDefault(LockWord lw) {
    return LockWord().GetValue() == lw.GetValue();
  }

  static LockWord Default() {
    return LockWord();
  }

  enum LockState {
    kUnlocked,    // No lock owners.
    kThinLocked,  // Single uncontended owner.
    kFatLocked,   // See associated monitor.
    kHashCode,    // Lock word contains an identity hash.
    kForwardingAddress,  // Lock word contains the forwarding address of an object.
  };

  LockState GetState() const {
    CheckReadBarrierState();
    if ((!kUseReadBarrier && UNLIKELY(value_ == 0)) ||
        (kUseReadBarrier && UNLIKELY((value_ & kGCStateMaskShiftedToggled) == 0))) {
      return kUnlocked;
    } else {
      uint32_t internal_state = (value_ >> kStateShift) & kStateMask;
      switch (internal_state) {
        case kStateThinOrUnlocked:
          return kThinLocked;
        case kStateHash:
          return kHashCode;
        case kStateForwardingAddress:
          return kForwardingAddress;
        default:
          DCHECK_EQ(internal_state, static_cast<uint32_t>(kStateFat));
          return kFatLocked;
      }
    }
  }

  uint32_t ReadBarrierState() const {
    return (value_ >> kReadBarrierStateShift) & kReadBarrierStateMask;
  }

  uint32_t GCState() const {
    return (value_ & kGCStateMaskShifted) >> kGCStateShift;
  }

  void SetReadBarrierState(uint32_t rb_state) {
    DCHECK_EQ(rb_state & ~kReadBarrierStateMask, 0U);
    DCHECK(rb_state == ReadBarrier::WhiteState() ||
           rb_state == ReadBarrier::GrayState()) << rb_state;
    DCHECK_NE(static_cast<uint32_t>(GetState()), static_cast<uint32_t>(kForwardingAddress));
    // Clear and or the bits.
    value_ &= ~(kReadBarrierStateMask << kReadBarrierStateShift);
    value_ |= (rb_state & kReadBarrierStateMask) << kReadBarrierStateShift;
  }


  uint32_t MarkBitState() const {
    return (value_ >> kMarkBitStateShift) & kMarkBitStateMask;
  }

  void SetMarkBitState(uint32_t mark_bit) {
    DCHECK_EQ(mark_bit & ~kMarkBitStateMask, 0U);
    DCHECK_NE(static_cast<uint32_t>(GetState()), static_cast<uint32_t>(kForwardingAddress));
    // Clear and or the bits.
    value_ &= kMarkBitStateMaskShiftedToggled;
    value_ |= mark_bit << kMarkBitStateShift;
  }

  // Return the owner thin lock thread id.
  uint32_t ThinLockOwner() const;

  // Return the number of times a lock value has been locked.
  uint32_t ThinLockCount() const;

  // Return the Monitor encoded in a fat lock.
  Monitor* FatLockMonitor() const;

  // Return the forwarding address stored in the monitor.
  size_t ForwardingAddress() const;

  // Constructor a lock word for inflation to use a Monitor.
  LockWord(Monitor* mon, uint32_t gc_state);

  // Return the hash code stored in the lock word, must be kHashCode state.
  int32_t GetHashCode() const;

  template <bool kIncludeReadBarrierState>
  static bool Equal(LockWord lw1, LockWord lw2) {
    if (kIncludeReadBarrierState) {
      return lw1.GetValue() == lw2.GetValue();
    }
    return lw1.GetValueWithoutGCState() == lw2.GetValueWithoutGCState();
  }

  void Dump(std::ostream& os) {
    os << "LockWord:" << std::hex << value_;
  }

 private:
  // Default constructor with no lock ownership.
  LockWord();

  explicit LockWord(uint32_t val) : value_(val) {
    // Make sure adding the overflow causes an overflow.
    constexpr uint64_t overflow = static_cast<uint64_t>(kStateForwardingAddressShifted) +
        static_cast<uint64_t>(kStateForwardingAddressOverflow);
    constexpr bool is_larger = overflow > static_cast<uint64_t>(0xFFFFFFFF);
    static_assert(is_larger, "should have overflowed");
    static_assert(
         (~kStateForwardingAddress & kStateMask) == 0,
        "READ_BARRIER_MARK_REG relies on the forwarding address state being only one bits");
    CheckReadBarrierState();
  }

  // Disallow this in favor of explicit Equal() with the
  // kIncludeReadBarrierState param to make clients be aware of the
  // read barrier state.
  bool operator==(const LockWord& rhs) = delete;

  void CheckReadBarrierState() const {
    if (kIsDebugBuild && ((value_ >> kStateShift) & kStateMask) != kStateForwardingAddress) {
      uint32_t rb_state = ReadBarrierState();
      if (!kUseReadBarrier) {
        DCHECK_EQ(rb_state, 0U);
      } else {
        DCHECK(rb_state == ReadBarrier::WhiteState() ||
               rb_state == ReadBarrier::GrayState()) << rb_state;
      }
    }
  }

  // Note GetValue() includes the read barrier bits and comparing (==)
  // GetValue() between two lock words to compare the lock states may
  // not work. Prefer Equal() or GetValueWithoutReadBarrierState().
  uint32_t GetValue() const {
    CheckReadBarrierState();
    return value_;
  }

  uint32_t GetValueWithoutGCState() const {
    CheckReadBarrierState();
    return value_ & kGCStateMaskShiftedToggled;
  }

  // Only Object should be converting LockWords to/from uints.
  friend class mirror::Object;

  // The encoded value holding all the state.
  uint32_t value_;
};
std::ostream& operator<<(std::ostream& os, const LockWord::LockState& code);

}  // namespace art


#endif  // ART_RUNTIME_LOCK_WORD_H_
