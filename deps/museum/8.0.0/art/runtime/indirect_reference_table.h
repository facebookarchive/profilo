/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef ART_RUNTIME_INDIRECT_REFERENCE_TABLE_H_
#define ART_RUNTIME_INDIRECT_REFERENCE_TABLE_H_

#include <stdint.h>

#include <iosfwd>
#include <limits>
#include <string>

#include "base/bit_utils.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "obj_ptr.h"
#include "object_callbacks.h"
#include "offsets.h"
#include "read_barrier_option.h"

namespace art {

class RootInfo;

namespace mirror {
class Object;
}  // namespace mirror

class MemMap;

// Maintain a table of indirect references.  Used for local/global JNI references.
//
// The table contains object references, where the strong (local/global) references are part of the
// GC root set (but not the weak global references). When an object is added we return an
// IndirectRef that is not a valid pointer but can be used to find the original value in O(1) time.
// Conversions to and from indirect references are performed on upcalls and downcalls, so they need
// to be very fast.
//
// To be efficient for JNI local variable storage, we need to provide operations that allow us to
// operate on segments of the table, where segments are pushed and popped as if on a stack. For
// example, deletion of an entry should only succeed if it appears in the current segment, and we
// want to be able to strip off the current segment quickly when a method returns. Additions to the
// table must be made in the current segment even if space is available in an earlier area.
//
// A new segment is created when we call into native code from interpreted code, or when we handle
// the JNI PushLocalFrame function.
//
// The GC must be able to scan the entire table quickly.
//
// In summary, these must be very fast:
//  - adding or removing a segment
//  - adding references to a new segment
//  - converting an indirect reference back to an Object
// These can be a little slower, but must still be pretty quick:
//  - adding references to a "mature" segment
//  - removing individual references
//  - scanning the entire table straight through
//
// If there's more than one segment, we don't guarantee that the table will fill completely before
// we fail due to lack of space. We do ensure that the current segment will pack tightly, which
// should satisfy JNI requirements (e.g. EnsureLocalCapacity).
//
// Only SynchronizedGet is synchronized.

// Indirect reference definition.  This must be interchangeable with JNI's jobject, and it's
// convenient to let null be null, so we use void*.
//
// We need a (potentially) large table index and a 2-bit reference type (global, local, weak
// global). We also reserve some bits to be used to detect stale indirect references: we put a
// serial number in the extra bits, and keep a copy of the serial number in the table. This requires
// more memory and additional memory accesses on add/get, but is moving-GC safe. It will catch
// additional problems, e.g.: create iref1 for obj, delete iref1, create iref2 for same obj,
// lookup iref1. A pattern based on object bits will miss this.
typedef void* IndirectRef;

// Indirect reference kind, used as the two low bits of IndirectRef.
//
// For convenience these match up with enum jobjectRefType from jni.h.
enum IndirectRefKind {
  kHandleScopeOrInvalid = 0,           // <<stack indirect reference table or invalid reference>>
  kLocal                = 1,           // <<local reference>>
  kGlobal               = 2,           // <<global reference>>
  kWeakGlobal           = 3,           // <<weak global reference>>
  kLastKind             = kWeakGlobal
};
std::ostream& operator<<(std::ostream& os, const IndirectRefKind& rhs);
const char* GetIndirectRefKindString(const IndirectRefKind& kind);

// Table definition.
//
// For the global reference table, the expected common operations are adding a new entry and
// removing a recently-added entry (usually the most-recently-added entry).  For JNI local
// references, the common operations are adding a new entry and removing an entire table segment.
//
// If we delete entries from the middle of the list, we will be left with "holes".  We track the
// number of holes so that, when adding new elements, we can quickly decide to do a trivial append
// or go slot-hunting.
//
// When the top-most entry is removed, any holes immediately below it are also removed. Thus,
// deletion of an entry may reduce "top_index" by more than one.
//
// To get the desired behavior for JNI locals, we need to know the bottom and top of the current
// "segment". The top is managed internally, and the bottom is passed in as a function argument.
// When we call a native method or push a local frame, the current top index gets pushed on, and
// serves as the new bottom. When we pop a frame off, the value from the stack becomes the new top
// index, and the value stored in the previous frame becomes the new bottom.
//
// Holes are being locally cached for the segment. Otherwise we'd have to pass bottom index and
// number of holes, which restricts us to 16 bits for the top index. The value is cached within the
// table. To avoid code in generated JNI transitions, which implicitly form segments, the code for
// adding and removing references needs to detect the change of a segment. Helper fields are used
// for this detection.
//
// Common alternative implementation: make IndirectRef a pointer to the actual reference slot.
// Instead of getting a table and doing a lookup, the lookup can be done instantly. Operations like
// determining the type and deleting the reference are more expensive because the table must be
// hunted for (i.e. you have to do a pointer comparison to see which table it's in), you can't move
// the table when expanding it (so realloc() is out), and tricks like serial number checking to
// detect stale references aren't possible (though we may be able to get similar benefits with other
// approaches).
//
// TODO: consider a "lastDeleteIndex" for quick hole-filling when an add immediately follows a
// delete; must invalidate after segment pop might be worth only using it for JNI globals.
//
// TODO: may want completely different add/remove algorithms for global and local refs to improve
// performance.  A large circular buffer might reduce the amortized cost of adding global
// references.

// The state of the current segment. We only store the index. Splitting it for index and hole
// count restricts the range too much.
struct IRTSegmentState {
  uint32_t top_index;
};

// Use as initial value for "cookie", and when table has only one segment.
static constexpr IRTSegmentState kIRTFirstSegment = { 0 };

// Try to choose kIRTPrevCount so that sizeof(IrtEntry) is a power of 2.
// Contains multiple entries but only one active one, this helps us detect use after free errors
// since the serial stored in the indirect ref wont match.
static constexpr size_t kIRTPrevCount = kIsDebugBuild ? 7 : 3;

class IrtEntry {
 public:
  void Add(ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  GcRoot<mirror::Object>* GetReference() {
    DCHECK_LT(serial_, kIRTPrevCount);
    return &references_[serial_];
  }

  const GcRoot<mirror::Object>* GetReference() const {
    DCHECK_LT(serial_, kIRTPrevCount);
    return &references_[serial_];
  }

  uint32_t GetSerial() const {
    return serial_;
  }

  void SetReference(ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  uint32_t serial_;
  GcRoot<mirror::Object> references_[kIRTPrevCount];
};
static_assert(sizeof(IrtEntry) == (1 + kIRTPrevCount) * sizeof(uint32_t),
              "Unexpected sizeof(IrtEntry)");
static_assert(IsPowerOfTwo(sizeof(IrtEntry)), "Unexpected sizeof(IrtEntry)");

class IrtIterator {
 public:
  IrtIterator(IrtEntry* table, size_t i, size_t capacity) REQUIRES_SHARED(Locks::mutator_lock_)
      : table_(table), i_(i), capacity_(capacity) {
  }

  IrtIterator& operator++() REQUIRES_SHARED(Locks::mutator_lock_) {
    ++i_;
    return *this;
  }

  GcRoot<mirror::Object>* operator*() REQUIRES_SHARED(Locks::mutator_lock_) {
    // This does not have a read barrier as this is used to visit roots.
    return table_[i_].GetReference();
  }

  bool equals(const IrtIterator& rhs) const {
    return (i_ == rhs.i_ && table_ == rhs.table_);
  }

 private:
  IrtEntry* const table_;
  size_t i_;
  const size_t capacity_;
};

bool inline operator==(const IrtIterator& lhs, const IrtIterator& rhs) {
  return lhs.equals(rhs);
}

bool inline operator!=(const IrtIterator& lhs, const IrtIterator& rhs) {
  return !lhs.equals(rhs);
}

class IndirectReferenceTable {
 public:
  enum class ResizableCapacity {
    kNo,
    kYes
  };

  // WARNING: Construction of the IndirectReferenceTable may fail.
  // error_msg must not be null. If error_msg is set by the constructor, then
  // construction has failed and the IndirectReferenceTable will be in an
  // invalid state. Use IsValid to check whether the object is in an invalid
  // state.
  IndirectReferenceTable(size_t max_count,
                         IndirectRefKind kind,
                         ResizableCapacity resizable,
                         std::string* error_msg);

  ~IndirectReferenceTable();

  /*
   * Checks whether construction of the IndirectReferenceTable succeeded.
   *
   * This object must only be used if IsValid() returns true. It is safe to
   * call IsValid from multiple threads without locking or other explicit
   * synchronization.
   */
  bool IsValid() const;

  // Add a new entry. "obj" must be a valid non-null object reference. This function will
  // abort if the table is full (max entries reached, or expansion failed).
  IndirectRef Add(IRTSegmentState previous_state, ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given an IndirectRef in the table, return the Object it refers to.
  //
  // This function may abort under error conditions.
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<mirror::Object> Get(IndirectRef iref) const REQUIRES_SHARED(Locks::mutator_lock_)
      ALWAYS_INLINE;

  // Synchronized get which reads a reference, acquiring a lock if necessary.
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<mirror::Object> SynchronizedGet(IndirectRef iref) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return Get<kReadBarrierOption>(iref);
  }

  // Updates an existing indirect reference to point to a new object.
  void Update(IndirectRef iref, ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Remove an existing entry.
  //
  // If the entry is not between the current top index and the bottom index
  // specified by the cookie, we don't remove anything.  This is the behavior
  // required by JNI's DeleteLocalRef function.
  //
  // Returns "false" if nothing was removed.
  bool Remove(IRTSegmentState previous_state, IndirectRef iref);

  void AssertEmpty() REQUIRES_SHARED(Locks::mutator_lock_);

  void Dump(std::ostream& os) const REQUIRES_SHARED(Locks::mutator_lock_);

  // Return the #of entries in the entire table.  This includes holes, and
  // so may be larger than the actual number of "live" entries.
  size_t Capacity() const {
    return segment_state_.top_index;
  }

  // Note IrtIterator does not have a read barrier as it's used to visit roots.
  IrtIterator begin() {
    return IrtIterator(table_, 0, Capacity());
  }

  IrtIterator end() {
    return IrtIterator(table_, Capacity(), Capacity());
  }

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info)
      REQUIRES_SHARED(Locks::mutator_lock_);

  IRTSegmentState GetSegmentState() const {
    return segment_state_;
  }

  void SetSegmentState(IRTSegmentState new_state);

  static Offset SegmentStateOffset(size_t pointer_size ATTRIBUTE_UNUSED) {
    // Note: Currently segment_state_ is at offset 0. We're testing the expected value in
    //       jni_internal_test to make sure it stays correct. It is not OFFSETOF_MEMBER, as that
    //       is not pointer-size-safe.
    return Offset(0);
  }

  // Release pages past the end of the table that may have previously held references.
  void Trim() REQUIRES_SHARED(Locks::mutator_lock_);

  // Determine what kind of indirect reference this is. Opposite of EncodeIndirectRefKind.
  ALWAYS_INLINE static inline IndirectRefKind GetIndirectRefKind(IndirectRef iref) {
    return DecodeIndirectRefKind(reinterpret_cast<uintptr_t>(iref));
  }

 private:
  static constexpr size_t kSerialBits = MinimumBitsToStore(kIRTPrevCount);
  static constexpr uint32_t kShiftedSerialMask = (1u << kSerialBits) - 1;

  static constexpr size_t kKindBits = MinimumBitsToStore(
      static_cast<uint32_t>(IndirectRefKind::kLastKind));
  static constexpr uint32_t kKindMask = (1u << kKindBits) - 1;

  static constexpr uintptr_t EncodeIndex(uint32_t table_index) {
    static_assert(sizeof(IndirectRef) == sizeof(uintptr_t), "Unexpected IndirectRef size");
    DCHECK_LE(MinimumBitsToStore(table_index), BitSizeOf<uintptr_t>() - kSerialBits - kKindBits);
    return (static_cast<uintptr_t>(table_index) << kKindBits << kSerialBits);
  }
  static constexpr uint32_t DecodeIndex(uintptr_t uref) {
    return static_cast<uint32_t>((uref >> kKindBits) >> kSerialBits);
  }

  static constexpr uintptr_t EncodeIndirectRefKind(IndirectRefKind kind) {
    return static_cast<uintptr_t>(kind);
  }
  static constexpr IndirectRefKind DecodeIndirectRefKind(uintptr_t uref) {
    return static_cast<IndirectRefKind>(uref & kKindMask);
  }

  static constexpr uintptr_t EncodeSerial(uint32_t serial) {
    DCHECK_LE(MinimumBitsToStore(serial), kSerialBits);
    return serial << kKindBits;
  }
  static constexpr uint32_t DecodeSerial(uintptr_t uref) {
    return static_cast<uint32_t>(uref >> kKindBits) & kShiftedSerialMask;
  }

  constexpr uintptr_t EncodeIndirectRef(uint32_t table_index, uint32_t serial) const {
    DCHECK_LT(table_index, max_entries_);
    return EncodeIndex(table_index) | EncodeSerial(serial) | EncodeIndirectRefKind(kind_);
  }

  static void ConstexprChecks();

  // Extract the table index from an indirect reference.
  ALWAYS_INLINE static uint32_t ExtractIndex(IndirectRef iref) {
    return DecodeIndex(reinterpret_cast<uintptr_t>(iref));
  }

  IndirectRef ToIndirectRef(uint32_t table_index) const {
    DCHECK_LT(table_index, max_entries_);
    uint32_t serial = table_[table_index].GetSerial();
    return reinterpret_cast<IndirectRef>(EncodeIndirectRef(table_index, serial));
  }

  // Resize the backing table. Currently must be larger than the current size.
  bool Resize(size_t new_size, std::string* error_msg);

  void RecoverHoles(IRTSegmentState from);

  // Abort if check_jni is not enabled. Otherwise, just log as an error.
  static void AbortIfNoCheckJNI(const std::string& msg);

  /* extra debugging checks */
  bool GetChecked(IndirectRef) const REQUIRES_SHARED(Locks::mutator_lock_);
  bool CheckEntry(const char*, IndirectRef, uint32_t) const;

  /// semi-public - read/write by jni down calls.
  IRTSegmentState segment_state_;

  // Mem map where we store the indirect refs.
  std::unique_ptr<MemMap> table_mem_map_;
  // bottom of the stack. Do not directly access the object references
  // in this as they are roots. Use Get() that has a read barrier.
  IrtEntry* table_;
  // bit mask, ORed into all irefs.
  const IndirectRefKind kind_;

  // max #of entries allowed (modulo resizing).
  size_t max_entries_;

  // Some values to retain old behavior with holes. Description of the algorithm is in the .cc
  // file.
  // TODO: Consider other data structures for compact tables, e.g., free lists.
  size_t current_num_holes_;
  IRTSegmentState last_known_previous_state_;

  // Whether the table's capacity may be resized. As there are no locks used, it is the caller's
  // responsibility to ensure thread-safety.
  ResizableCapacity resizable_;
};

}  // namespace art

#endif  // ART_RUNTIME_INDIRECT_REFERENCE_TABLE_H_
