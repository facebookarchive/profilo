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

#ifndef ART_RUNTIME_READ_BARRIER_INL_H_
#define ART_RUNTIME_READ_BARRIER_INL_H_

#include "read_barrier.h"

#include "gc/collector/concurrent_copying.h"
#include "gc/heap.h"
#include "mirror/object_reference.h"
#include "mirror/reference.h"
#include "runtime.h"
#include "utils.h"

namespace art {

template <typename MirrorType, ReadBarrierOption kReadBarrierOption, bool kMaybeDuringStartup>
inline MirrorType* ReadBarrier::Barrier(
    mirror::Object* obj, MemberOffset offset, mirror::HeapReference<MirrorType>* ref_addr) {
  const bool with_read_barrier = kReadBarrierOption == kWithReadBarrier;
  if (with_read_barrier && kUseBakerReadBarrier) {
    // The higher bits of the rb ptr, rb_ptr_high_bits (must be zero)
    // is used to create artificial data dependency from the is_gray
    // load to the ref field (ptr) load to avoid needing a load-load
    // barrier between the two.
    uintptr_t rb_ptr_high_bits;
    bool is_gray = HasGrayReadBarrierPointer(obj, &rb_ptr_high_bits);
    ref_addr = reinterpret_cast<mirror::HeapReference<MirrorType>*>(
        rb_ptr_high_bits | reinterpret_cast<uintptr_t>(ref_addr));
    MirrorType* ref = ref_addr->AsMirrorPtr();
    if (is_gray) {
      // Slow-path.
      ref = reinterpret_cast<MirrorType*>(Mark(ref));
    }
    if (kEnableReadBarrierInvariantChecks) {
      CHECK_EQ(rb_ptr_high_bits, 0U) << obj << " rb_ptr=" << obj->GetReadBarrierPointer();
    }
    AssertToSpaceInvariant(obj, offset, ref);
    return ref;
  } else if (with_read_barrier && kUseBrooksReadBarrier) {
    // To be implemented.
    return ref_addr->AsMirrorPtr();
  } else if (with_read_barrier && kUseTableLookupReadBarrier) {
    MirrorType* ref = ref_addr->AsMirrorPtr();
    MirrorType* old_ref = ref;
    // The heap or the collector can be null at startup. TODO: avoid the need for this null check.
    gc::Heap* heap = Runtime::Current()->GetHeap();
    if (heap != nullptr && heap->GetReadBarrierTable()->IsSet(old_ref)) {
      ref = reinterpret_cast<MirrorType*>(Mark(old_ref));
      // Update the field atomically. This may fail if mutator updates before us, but it's ok.
      obj->CasFieldStrongSequentiallyConsistentObjectWithoutWriteBarrier<false, false>(
          offset, old_ref, ref);
    }
    AssertToSpaceInvariant(obj, offset, ref);
    return ref;
  } else {
    // No read barrier.
    return ref_addr->AsMirrorPtr();
  }
}

template <typename MirrorType, ReadBarrierOption kReadBarrierOption, bool kMaybeDuringStartup>
inline MirrorType* ReadBarrier::BarrierForRoot(MirrorType** root) {
  MirrorType* ref = *root;
  const bool with_read_barrier = kReadBarrierOption == kWithReadBarrier;
  if (with_read_barrier && kUseBakerReadBarrier) {
    if (kMaybeDuringStartup && IsDuringStartup()) {
      // During startup, the heap may not be initialized yet. Just
      // return the given ref.
      return ref;
    }
    // TODO: separate the read barrier code from the collector code more.
    if (Runtime::Current()->GetHeap()->ConcurrentCopyingCollector()->IsMarking()) {
      ref = reinterpret_cast<MirrorType*>(Mark(ref));
    }
    AssertToSpaceInvariant(nullptr, MemberOffset(0), ref);
    return ref;
  } else if (with_read_barrier && kUseBrooksReadBarrier) {
    // To be implemented.
    return ref;
  } else if (with_read_barrier && kUseTableLookupReadBarrier) {
    if (kMaybeDuringStartup && IsDuringStartup()) {
      // During startup, the heap may not be initialized yet. Just
      // return the given ref.
      return ref;
    }
    if (Runtime::Current()->GetHeap()->GetReadBarrierTable()->IsSet(ref)) {
      MirrorType* old_ref = ref;
      ref = reinterpret_cast<MirrorType*>(Mark(old_ref));
      // Update the field atomically. This may fail if mutator updates before us, but it's ok.
      Atomic<mirror::Object*>* atomic_root = reinterpret_cast<Atomic<mirror::Object*>*>(root);
      atomic_root->CompareExchangeStrongSequentiallyConsistent(old_ref, ref);
    }
    AssertToSpaceInvariant(nullptr, MemberOffset(0), ref);
    return ref;
  } else {
    return ref;
  }
}

// TODO: Reduce copy paste
template <typename MirrorType, ReadBarrierOption kReadBarrierOption, bool kMaybeDuringStartup>
inline MirrorType* ReadBarrier::BarrierForRoot(mirror::CompressedReference<MirrorType>* root) {
  MirrorType* ref = root->AsMirrorPtr();
  const bool with_read_barrier = kReadBarrierOption == kWithReadBarrier;
  if (with_read_barrier && kUseBakerReadBarrier) {
    if (kMaybeDuringStartup && IsDuringStartup()) {
      // During startup, the heap may not be initialized yet. Just
      // return the given ref.
      return ref;
    }
    // TODO: separate the read barrier code from the collector code more.
    if (Runtime::Current()->GetHeap()->ConcurrentCopyingCollector()->IsMarking()) {
      ref = reinterpret_cast<MirrorType*>(Mark(ref));
    }
    AssertToSpaceInvariant(nullptr, MemberOffset(0), ref);
    return ref;
  } else if (with_read_barrier && kUseBrooksReadBarrier) {
    // To be implemented.
    return ref;
  } else if (with_read_barrier && kUseTableLookupReadBarrier) {
    if (kMaybeDuringStartup && IsDuringStartup()) {
      // During startup, the heap may not be initialized yet. Just
      // return the given ref.
      return ref;
    }
    if (Runtime::Current()->GetHeap()->GetReadBarrierTable()->IsSet(ref)) {
      auto old_ref = mirror::CompressedReference<MirrorType>::FromMirrorPtr(ref);
      ref = reinterpret_cast<MirrorType*>(Mark(ref));
      auto new_ref = mirror::CompressedReference<MirrorType>::FromMirrorPtr(ref);
      // Update the field atomically. This may fail if mutator updates before us, but it's ok.
      auto* atomic_root =
          reinterpret_cast<Atomic<mirror::CompressedReference<MirrorType>>*>(root);
      atomic_root->CompareExchangeStrongSequentiallyConsistent(old_ref, new_ref);
    }
    AssertToSpaceInvariant(nullptr, MemberOffset(0), ref);
    return ref;
  } else {
    return ref;
  }
}

inline bool ReadBarrier::IsDuringStartup() {
  gc::Heap* heap = Runtime::Current()->GetHeap();
  if (heap == nullptr) {
    // During startup, the heap can be null.
    return true;
  }
  if (heap->CurrentCollectorType() != gc::kCollectorTypeCC) {
    // CC isn't running.
    return true;
  }
  gc::collector::ConcurrentCopying* collector = heap->ConcurrentCopyingCollector();
  if (collector == nullptr) {
    // During startup, the collector can be null.
    return true;
  }
  return false;
}

inline void ReadBarrier::AssertToSpaceInvariant(mirror::Object* obj, MemberOffset offset,
                                                mirror::Object* ref) {
  if (kEnableToSpaceInvariantChecks || kIsDebugBuild) {
    if (ref == nullptr || IsDuringStartup()) {
      return;
    }
    Runtime::Current()->GetHeap()->ConcurrentCopyingCollector()->
        AssertToSpaceInvariant(obj, offset, ref);
  }
}

inline mirror::Object* ReadBarrier::Mark(mirror::Object* obj) {
  return Runtime::Current()->GetHeap()->ConcurrentCopyingCollector()->Mark(obj);
}

inline bool ReadBarrier::HasGrayReadBarrierPointer(mirror::Object* obj,
                                                   uintptr_t* out_rb_ptr_high_bits) {
  mirror::Object* rb_ptr = obj->GetReadBarrierPointer();
  uintptr_t rb_ptr_bits = reinterpret_cast<uintptr_t>(rb_ptr);
  uintptr_t rb_ptr_low_bits = rb_ptr_bits & rb_ptr_mask_;
  if (kEnableReadBarrierInvariantChecks) {
    CHECK(rb_ptr_low_bits == white_ptr_ || rb_ptr_low_bits == gray_ptr_ ||
          rb_ptr_low_bits == black_ptr_)
        << "obj=" << obj << " rb_ptr=" << rb_ptr << " " << PrettyTypeOf(obj);
  }
  bool is_gray = rb_ptr_low_bits == gray_ptr_;
  // The high bits are supposed to be zero. We check this on the caller side.
  *out_rb_ptr_high_bits = rb_ptr_bits & ~rb_ptr_mask_;
  return is_gray;
}

}  // namespace art

#endif  // ART_RUNTIME_READ_BARRIER_INL_H_
