/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_OBJ_PTR_H_
#define ART_RUNTIME_OBJ_PTR_H_

#include <ostream>
#include <type_traits>

#include "base/macros.h"
#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "globals.h"

namespace art {

constexpr bool kObjPtrPoisoning = kIsDebugBuild;

// Value type representing a pointer to a mirror::Object of type MirrorType
// Pass kPoison as a template boolean for testing in non-debug builds.
// Since the cookie is thread based, it is not safe to share an ObjPtr between threads.
template<class MirrorType>
class ObjPtr {
  static constexpr size_t kCookieShift =
      sizeof(kHeapReferenceSize) * kBitsPerByte - kObjectAlignmentShift;
  static constexpr size_t kCookieBits = sizeof(uintptr_t) * kBitsPerByte - kCookieShift;
  static constexpr uintptr_t kCookieMask = (static_cast<uintptr_t>(1u) << kCookieBits) - 1;

  static_assert(kCookieBits >= kObjectAlignmentShift,
                "must have a least kObjectAlignmentShift bits");

 public:
  ALWAYS_INLINE ObjPtr() REQUIRES_SHARED(Locks::mutator_lock_) : reference_(0u) {}

  // Note: The following constructors allow implicit conversion. This simplifies code that uses
  //       them, e.g., for parameter passing. However, in general, implicit-conversion constructors
  //       are discouraged and detected by cpplint and clang-tidy. So mark these constructors
  //       as NOLINT (without category, as the categories are different).

  ALWAYS_INLINE ObjPtr(std::nullptr_t)  // NOLINT
      REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(0u) {}

  template <typename Type,
            typename = typename std::enable_if<std::is_base_of<MirrorType, Type>::value>::type>
  ALWAYS_INLINE ObjPtr(Type* ptr)  // NOLINT
      REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Encode(static_cast<MirrorType*>(ptr))) {
  }

  template <typename Type,
            typename = typename std::enable_if<std::is_base_of<MirrorType, Type>::value>::type>
  ALWAYS_INLINE ObjPtr(const ObjPtr<Type>& other)  // NOLINT
      REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Encode(static_cast<MirrorType*>(other.Ptr()))) {
  }

  template <typename Type,
            typename = typename std::enable_if<std::is_base_of<MirrorType, Type>::value>::type>
  ALWAYS_INLINE ObjPtr& operator=(const ObjPtr<Type>& other)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    reference_ = Encode(static_cast<MirrorType*>(other.Ptr()));
    return *this;
  }

  ALWAYS_INLINE ObjPtr& operator=(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    Assign(ptr);
    return *this;
  }

  ALWAYS_INLINE void Assign(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    reference_ = Encode(ptr);
  }

  ALWAYS_INLINE MirrorType* operator->() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr();
  }

  ALWAYS_INLINE bool IsNull() const {
    return reference_ == 0;
  }

  // Ptr makes sure that the object pointer is valid.
  ALWAYS_INLINE MirrorType* Ptr() const REQUIRES_SHARED(Locks::mutator_lock_) {
    AssertValid();
    return PtrUnchecked();
  }

  ALWAYS_INLINE bool IsValid() const REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void AssertValid() const REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE bool operator==(const ObjPtr& ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() == ptr.Ptr();
  }

  template <typename PointerType>
  ALWAYS_INLINE bool operator==(const PointerType* ptr) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() == ptr;
  }

  ALWAYS_INLINE bool operator==(std::nullptr_t) const {
    return IsNull();
  }

  ALWAYS_INLINE bool operator!=(const ObjPtr& ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() != ptr.Ptr();
  }

  template <typename PointerType>
  ALWAYS_INLINE bool operator!=(const PointerType* ptr) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() != ptr;
  }

  ALWAYS_INLINE bool operator!=(std::nullptr_t) const {
    return !IsNull();
  }

  // Ptr unchecked does not check that object pointer is valid. Do not use if you can avoid it.
  ALWAYS_INLINE MirrorType* PtrUnchecked() const {
    if (kObjPtrPoisoning) {
      return reinterpret_cast<MirrorType*>(
          static_cast<uintptr_t>(static_cast<uint32_t>(reference_ << kObjectAlignmentShift)));
    } else {
      return reinterpret_cast<MirrorType*>(reference_);
    }
  }

  // Static function to be friendly with null pointers.
  template <typename SourceType>
  static ObjPtr<MirrorType> DownCast(ObjPtr<SourceType> ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    static_assert(std::is_base_of<SourceType, MirrorType>::value,
                  "Target type must be a subtype of source type");
    return static_cast<MirrorType*>(ptr.Ptr());
  }

 private:
  // Trim off high bits of thread local cookie.
  ALWAYS_INLINE static uintptr_t TrimCookie(uintptr_t cookie) {
    return cookie & kCookieMask;
  }

  ALWAYS_INLINE uintptr_t GetCookie() const {
    return reference_ >> kCookieShift;
  }

  ALWAYS_INLINE static uintptr_t Encode(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_);
  // The encoded reference and cookie.
  uintptr_t reference_;
};

static_assert(std::is_trivially_copyable<ObjPtr<void>>::value,
              "ObjPtr should be trivially copyable");

// Hash function for stl data structures.
class HashObjPtr {
 public:
  template<class MirrorType>
  size_t operator()(const ObjPtr<MirrorType>& ptr) const NO_THREAD_SAFETY_ANALYSIS {
    return std::hash<MirrorType*>()(ptr.Ptr());
  }
};

template<class MirrorType, typename PointerType>
ALWAYS_INLINE bool operator==(const PointerType* a, const ObjPtr<MirrorType>& b)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return b == a;
}

template<class MirrorType>
ALWAYS_INLINE bool operator==(std::nullptr_t, const ObjPtr<MirrorType>& b) {
  return b == nullptr;
}

template<typename MirrorType, typename PointerType>
ALWAYS_INLINE bool operator!=(const PointerType* a, const ObjPtr<MirrorType>& b)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return b != a;
}

template<class MirrorType>
ALWAYS_INLINE bool operator!=(std::nullptr_t, const ObjPtr<MirrorType>& b) {
  return b != nullptr;
}

template<class MirrorType>
static inline ObjPtr<MirrorType> MakeObjPtr(MirrorType* ptr) {
  return ObjPtr<MirrorType>(ptr);
}

template<class MirrorType>
static inline ObjPtr<MirrorType> MakeObjPtr(ObjPtr<MirrorType> ptr) {
  return ObjPtr<MirrorType>(ptr);
}

template<class MirrorType>
ALWAYS_INLINE std::ostream& operator<<(std::ostream& os, ObjPtr<MirrorType> ptr);

}  // namespace art

#endif  // ART_RUNTIME_OBJ_PTR_H_
