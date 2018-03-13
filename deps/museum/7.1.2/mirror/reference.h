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

#ifndef ART_RUNTIME_MIRROR_REFERENCE_H_
#define ART_RUNTIME_MIRROR_REFERENCE_H_

#include "class.h"
#include "gc_root.h"
#include "object.h"
#include "object_callbacks.h"
#include "read_barrier_option.h"
#include "runtime.h"
#include "thread.h"

namespace art {

namespace gc {

class ReferenceProcessor;
class ReferenceQueue;

}  // namespace gc

struct ReferenceOffsets;
struct FinalizerReferenceOffsets;

namespace mirror {

// C++ mirror of java.lang.ref.Reference
class MANAGED Reference : public Object {
 public:
  // Size of java.lang.ref.Reference.class.
  static uint32_t ClassSize(size_t pointer_size);

  // Size of an instance of java.lang.ref.Reference.
  static constexpr uint32_t InstanceSize() {
    return sizeof(Reference);
  }

  static MemberOffset PendingNextOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, pending_next_);
  }
  static MemberOffset QueueOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, queue_);
  }
  static MemberOffset QueueNextOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, queue_next_);
  }
  static MemberOffset ReferentOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Reference, referent_);
  }
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  Object* GetReferent() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObjectVolatile<Object, kDefaultVerifyFlags, kReadBarrierOption>(
        ReferentOffset());
  }
  template<bool kTransactionActive>
  void SetReferent(Object* referent) SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldObjectVolatile<kTransactionActive>(ReferentOffset(), referent);
  }
  template<bool kTransactionActive>
  void ClearReferent() SHARED_REQUIRES(Locks::mutator_lock_) {
    SetFieldObjectVolatile<kTransactionActive>(ReferentOffset(), nullptr);
  }

  Reference* GetPendingNext() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObject<Reference>(PendingNextOffset());
  }

  void SetPendingNext(Reference* pending_next)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    if (Runtime::Current()->IsActiveTransaction()) {
      SetFieldObject<true>(PendingNextOffset(), pending_next);
    } else {
      SetFieldObject<false>(PendingNextOffset(), pending_next);
    }
  }

  // Returns true if the reference's pendingNext is null, indicating it is
  // okay to process this reference.
  //
  // If pendingNext is not null, then one of the following cases holds:
  // 1. The reference has already been enqueued to a java ReferenceQueue. In
  // this case the referent should not be considered for reference processing
  // ever again.
  // 2. The reference is currently part of a list of references that may
  // shortly be enqueued on a java ReferenceQueue. In this case the reference
  // should not be processed again until and unless the reference has been
  // removed from the list after having determined the reference is not ready
  // to be enqueued on a java ReferenceQueue.
  bool IsUnprocessed() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetPendingNext() == nullptr;
  }

  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  static Class* GetJavaLangRefReference() SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(!java_lang_ref_Reference_.IsNull());
    return java_lang_ref_Reference_.Read<kReadBarrierOption>();
  }
  static void SetClass(Class* klass);
  static void ResetClass();
  static void VisitRoots(RootVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  // Note: This avoids a read barrier, it should only be used by the GC.
  HeapReference<Object>* GetReferentReferenceAddr() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObjectReferenceAddr<kDefaultVerifyFlags>(ReferentOffset());
  }

  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  HeapReference<Reference> pending_next_;
  HeapReference<Object> queue_;
  HeapReference<Reference> queue_next_;
  HeapReference<Object> referent_;  // Note this is Java volatile:

  static GcRoot<Class> java_lang_ref_Reference_;

  friend struct art::ReferenceOffsets;  // for verifying offset information
  friend class gc::ReferenceProcessor;
  friend class gc::ReferenceQueue;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Reference);
};

// C++ mirror of java.lang.ref.FinalizerReference
class MANAGED FinalizerReference : public Reference {
 public:
  static MemberOffset ZombieOffset() {
    return OFFSET_OF_OBJECT_MEMBER(FinalizerReference, zombie_);
  }

  template<bool kTransactionActive>
  void SetZombie(Object* zombie) SHARED_REQUIRES(Locks::mutator_lock_) {
    return SetFieldObjectVolatile<kTransactionActive>(ZombieOffset(), zombie);
  }
  Object* GetZombie() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObjectVolatile<Object>(ZombieOffset());
  }

 private:
  HeapReference<FinalizerReference> next_;
  HeapReference<FinalizerReference> prev_;
  HeapReference<Object> zombie_;

  friend struct art::FinalizerReferenceOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(FinalizerReference);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_REFERENCE_H_
