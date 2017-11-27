/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_DEBUG_STACK_H_
#define ART_RUNTIME_BASE_DEBUG_STACK_H_

#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {

// Helper classes for reference counting to enforce construction/destruction order and
// usage of the top element of a stack in debug mode with no overhead in release mode.

// Reference counter. No references allowed in destructor or in explicitly called CheckNoRefs().
template <bool kIsDebug>
class DebugStackRefCounterImpl;
// Reference. Allows an explicit check that it's the top reference.
template <bool kIsDebug>
class DebugStackReferenceImpl;
// Indirect top reference. Checks that the reference is the top reference when used.
template <bool kIsDebug>
class DebugStackIndirectTopRefImpl;

typedef DebugStackRefCounterImpl<kIsDebugBuild> DebugStackRefCounter;
typedef DebugStackReferenceImpl<kIsDebugBuild> DebugStackReference;
typedef DebugStackIndirectTopRefImpl<kIsDebugBuild> DebugStackIndirectTopRef;

// Non-debug mode specializations. This should be optimized away.

template <>
class DebugStackRefCounterImpl<false> {
 public:
  size_t IncrementRefCount() { return 0u; }
  void DecrementRefCount() { }
  size_t GetRefCount() const { return 0u; }
  void CheckNoRefs() const { }
};

template <>
class DebugStackReferenceImpl<false> {
 public:
  explicit DebugStackReferenceImpl(DebugStackRefCounterImpl<false>* counter) { UNUSED(counter); }
  DebugStackReferenceImpl(const DebugStackReferenceImpl& other) = default;
  DebugStackReferenceImpl& operator=(const DebugStackReferenceImpl& other) = default;
  void CheckTop() { }
};

template <>
class DebugStackIndirectTopRefImpl<false> {
 public:
  explicit DebugStackIndirectTopRefImpl(DebugStackReferenceImpl<false>* ref) { UNUSED(ref); }
  DebugStackIndirectTopRefImpl(const DebugStackIndirectTopRefImpl& other) = default;
  DebugStackIndirectTopRefImpl& operator=(const DebugStackIndirectTopRefImpl& other) = default;
  void CheckTop() { }
};

// Debug mode versions.

template <bool kIsDebug>
class DebugStackRefCounterImpl {
 public:
  DebugStackRefCounterImpl() : ref_count_(0u) { }
  ~DebugStackRefCounterImpl() { CheckNoRefs(); }
  size_t IncrementRefCount() { return ++ref_count_; }
  void DecrementRefCount() { --ref_count_; }
  size_t GetRefCount() const { return ref_count_; }
  void CheckNoRefs() const { CHECK_EQ(ref_count_, 0u); }

 private:
  size_t ref_count_;
};

template <bool kIsDebug>
class DebugStackReferenceImpl {
 public:
  explicit DebugStackReferenceImpl(DebugStackRefCounterImpl<kIsDebug>* counter)
    : counter_(counter), ref_count_(counter->IncrementRefCount()) {
  }
  DebugStackReferenceImpl(const DebugStackReferenceImpl& other)
    : counter_(other.counter_), ref_count_(counter_->IncrementRefCount()) {
  }
  DebugStackReferenceImpl& operator=(const DebugStackReferenceImpl& other) {
    CHECK(counter_ == other.counter_);
    return *this;
  }
  ~DebugStackReferenceImpl() { counter_->DecrementRefCount(); }
  void CheckTop() { CHECK_EQ(counter_->GetRefCount(), ref_count_); }

 private:
  DebugStackRefCounterImpl<true>* counter_;
  size_t ref_count_;
};

template <bool kIsDebug>
class DebugStackIndirectTopRefImpl {
 public:
  explicit DebugStackIndirectTopRefImpl(DebugStackReferenceImpl<kIsDebug>* ref)
      : ref_(ref) {
    CheckTop();
  }
  DebugStackIndirectTopRefImpl(const DebugStackIndirectTopRefImpl& other)
      : ref_(other.ref_) {
    CheckTop();
  }
  DebugStackIndirectTopRefImpl& operator=(const DebugStackIndirectTopRefImpl& other) {
    CHECK(ref_ == other.ref_);
    CheckTop();
    return *this;
  }
  ~DebugStackIndirectTopRefImpl() {
    CheckTop();
  }
  void CheckTop() {
    ref_->CheckTop();
  }

 private:
  DebugStackReferenceImpl<kIsDebug>* ref_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_DEBUG_STACK_H_
