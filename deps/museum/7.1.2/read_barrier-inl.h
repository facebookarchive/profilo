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

#include "gc/collector/concurrent_copying-inl.h"
#include "gc/heap.h"
#include "mirror/object_reference.h"
#include "mirror/reference.h"
#include "runtime.h"
#include "utils.h"

namespace art {

template <typename MirrorType, ReadBarrierOption kReadBarrierOption, bool kAlwaysUpdateField>
inline MirrorType* ReadBarrier::Barrier(
    mirror::Object* obj, MemberOffset offset, mirror::HeapReference<MirrorType>* ref_addr) {
  constexpr bool with_read_barrier = kReadBarrierOption == kWithReadBarrier;
  if (kUseReadBarrier && with_read_barrier) {
    if (kIsDebugBuild) {
      Thread* const self = Thread::Current();
      if (self != nullptr) {
        CHECK_EQ(self->GetDebugDisallowReadBarrierCount(), 0u);
      }
    }
    if (kUseBakerReadBarrier) {
      // The higher bits of the rb_ptr, rb_ptr_high_bits (must be zero)
      // is used to create artificial data dependency from the is_gray
      // load to the ref field (ptr) load to avoid needing a load-load
      // barrier between the two.
      uintptr_t rb_ptr_high_bits;
      bool is_gray = HasGrayReadBarrierPointer(obj, &rb_ptr_high_bits);
      ref_addr = reinterpret_cast<mirror::HeapReference<MirrorType>*>(
          rb_ptr_high_bits | reinterpret_cast<uintptr_t>(ref_addr));
      MirrorType* ref = ref_addr->AsMirrorPtr();
      MirrorType* old_ref = ref;
      if (is_gray) {
        // Slow-path.
        ref = reinterpret_cast<MirrorType*>(Mark(ref));
        // If kAlwaysUpdateField is true, update the field atomically. This may fail if mutator
        // updates before us, but it's ok.
        if (kAlwaysUpdateField && ref != old_ref) {
          obj->CasFieldStrongRelaxedObjectWithoutWriteBarrier<false, false>(
              offset, old_ref, ref);
        }
      }
      if (kEnableReadBarrierInvariantChecks) {
        CHECK_EQ(rb_ptr_high_bits, 0U) << obj << " rb_ptr=" << obj->GetReadBarrierPointer();
      }
      AssertToSpaceInvariant(obj, offset, ref);
      return ref;
    } else if (kUseBrooksReadBarrier) {
      // To be implemented.
      return ref_addr->AsMirrorPtr();
    } else if (kUseTableLookupReadBarrier) {
      MirrorType* ref = ref_addr->AsMirrorPtr();
      MirrorType* old_ref = ref;
      // The heap or the collector can be null at startup. TODO: avoid the need for this null check.
      gc::Heap* heap = Runtime::Current()->GetHeap();
      if (heap != nullptr && heap->GetReadBarrierTable()->IsSet(old_ref)) {
        ref = reinterpret_cast<MirrorType*>(Mark(old_ref));
        // Update the field atomically. This may fail if mutator updates before us, but it's ok.
        if (ref != old_ref) {
          obj->CasFieldStrongRelaxedObjectWithoutWriteBarrier<false, false>(
              offset, old_ref, ref);
        }
      }
      AssertToSpaceInvariant(obj, offset, ref);
      return ref;
    } else {
      LOG(FATAL) << "Unexpected read barrier type";
      UNREACHABLE();
    }
  } else {
    // No read barrier.
    return ref_addr->AsMirrorPtr();
  }
}

template <typename MirrorType, ReadBarrierOption kReadBarrierOption>
inline MirrorType* ReadBarrier::BarrierForRoot(MirrorType** root,
                                               GcRootSource* gc_root_source) {
  MirrorType* ref = *root;
  const bool with_read_barrier = kReadBarrierOption == kWithReadBarrier;
  if (kUseReadBarrier && with_read_barrier) {
    if (kIsDebugBuild) {
      Thread* const self = Thread::Current();
      if (self != nullptr) {
        CHECK_EQ(self->GetDebugDisallowReadBarrierCount(), 0u);
      }
    }
    if (kUseBakerReadBarrier) {
      // TODO: separate the read barrier code from the collector code more.
      Thread* self = Thread::Current();
      if (self != nullptr && self->GetIsGcMarking()) {
        ref = reinterpret_cast<MirrorType*>(Mark(ref));
      }
      AssertToSpaceInvariant(gc_root_source, ref);
      return ref;
    } else if (kUseBrooksReadBarrier) {
      // To be implemented.
      return ref;
    } else if (kUseTableLookupReadBarrier) {
      Thread* self = Thread::Current();
      if (self != nullptr &&
          self->GetIsGcMarking() &&
          Runtime::Current()->GetHeap()->GetReadBarrierTable()->IsSet(ref)) {
        MirrorType* old_ref = ref;
        ref = reinterpret_cast<MirrorType*>(Mark(old_ref));
        // Update the field atomically. This may fail if mutator updates before us, but it's ok.
        if (ref != old_ref) {
          Atomic<mirror::Object*>* atomic_root = reinterpret_cast<Atomic<mirror::Object*>*>(root);
          atomic_root->CompareExchangeStrongRelaxed(old_ref, ref);
        }
      }
      AssertToSpaceInvariant(gc_root_source, ref);
      return ref;
    } else {
      LOG(FATAL) << "Unexpected read barrier type";
      UNREACHABLE();
    }
  } else {
    return ref;
  }
}

// TODO: Reduce copy paste
template <typename MirrorType, ReadBarrierOption kReadBarrierOption>
inline MirrorType* ReadBarrier::BarrierForRoot(mirror::CompressedReference<MirrorType>* root,
                                               GcRootSource* gc_root_source) {
  MirrorType* ref = root->AsMirrorPtr();
  const bool with_read_barrier = kReadBarrierOption == kWithReadBarrier;
  if (with_read_barrier && kUseBakerReadBarrier) {
    // TODO: separate the read barrier code from the collector code more.
    Thread* self = Thread::Current();
    if (self != nullptr && self->GetIsGcMarking()) {
      ref = reinterpret_cast<MirrorType*>(Mark(ref));
    }
    AssertToSpaceInvariant(gc_root_source, ref);
    return ref;
  } else if (with_read_barrier && kUseBrooksReadBarrier) {
    // To be implemented.
    return ref;
  } else if (with_read_barrier && kUseTableLookupReadBarrier) {
    Thread* self = Thread::Current();
    if (self != nullptr &&
        self->GetIsGcMarking() &&
        Runtime::Current()->GetHeap()->GetReadBarrierTable()->IsSet(ref)) {
      auto old_ref = mirror::CompressedReference<MirrorType>::FromMirrorPtr(ref);
      ref = reinterpret_cast<MirrorType*>(Mark(ref));
      auto new_ref = mirror::CompressedReference<MirrorType>::FromMirrorPtr(ref);
      // Update the field atomically. This may fail if mutator updates before us, but it's ok.
      if (new_ref.AsMirrorPtr() != old_ref.AsMirrorPtr()) {
        auto* atomic_root =
            reinterpret_cast<Atomic<mirror::CompressedReference<MirrorType>>*>(root);
        atomic_root->CompareExchangeStrongRelaxed(old_ref, new_ref);
      }
    }
    AssertToSpaceInvariant(gc_root_source, ref);
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

inline void ReadBarrier::AssertToSpaceInvariant(GcRootSource* gc_root_source,
                                                mirror::Object* ref) {
  if (kEnableToSpaceInvariantChecks || kIsDebugBuild) {
    if (ref == nullptr || IsDuringStartup()) {
      return;
    }
    Runtime::Current()->GetHeap()->ConcurrentCopyingCollector()->
        AssertToSpaceInvariant(gc_root_source, ref);
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
