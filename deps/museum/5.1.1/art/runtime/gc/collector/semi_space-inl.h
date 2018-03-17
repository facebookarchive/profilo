/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_SEMI_SPACE_INL_H_
#define ART_RUNTIME_GC_COLLECTOR_SEMI_SPACE_INL_H_

#include "semi_space.h"

#include "gc/accounting/heap_bitmap.h"
#include "mirror/object-inl.h"

namespace art {
namespace gc {
namespace collector {

class BitmapSetSlowPathVisitor {
 public:
  explicit BitmapSetSlowPathVisitor(SemiSpace* semi_space) : semi_space_(semi_space) {
  }

  void operator()(const mirror::Object* obj) const {
    CHECK(!semi_space_->to_space_->HasAddress(obj)) << "Marking " << obj << " in to_space_";
    // Marking a large object, make sure its aligned as a sanity check.
    CHECK(IsAligned<kPageSize>(obj));
  }

 private:
  SemiSpace* const semi_space_;
};

inline mirror::Object* SemiSpace::GetForwardingAddressInFromSpace(mirror::Object* obj) const {
  DCHECK(from_space_->HasAddress(obj));
  LockWord lock_word = obj->GetLockWord(false);
  if (lock_word.GetState() != LockWord::kForwardingAddress) {
    return nullptr;
  }
  return reinterpret_cast<mirror::Object*>(lock_word.ForwardingAddress());
}

// Used to mark and copy objects. Any newly-marked objects who are in the from space Get moved to
// the to-space and have their forward address updated. Objects which have been newly marked are
// pushed on the mark stack.
template<bool kPoisonReferences>
inline void SemiSpace::MarkObject(
    mirror::ObjectReference<kPoisonReferences, mirror::Object>* obj_ptr) {
  mirror::Object* obj = obj_ptr->AsMirrorPtr();
  if (obj == nullptr) {
    return;
  }
  if (kUseBakerOrBrooksReadBarrier) {
    // Verify all the objects have the correct forward pointer installed.
    obj->AssertReadBarrierPointer();
  }
  if (from_space_->HasAddress(obj)) {
    mirror::Object* forward_address = GetForwardingAddressInFromSpace(obj);
    // If the object has already been moved, return the new forward address.
    if (UNLIKELY(forward_address == nullptr)) {
      forward_address = MarkNonForwardedObject(obj);
      DCHECK(forward_address != nullptr);
      // Make sure to only update the forwarding address AFTER you copy the object so that the
      // monitor word doesn't Get stomped over.
      obj->SetLockWord(
          LockWord::FromForwardingAddress(reinterpret_cast<size_t>(forward_address)), false);
      // Push the object onto the mark stack for later processing.
      MarkStackPush(forward_address);
    }
    obj_ptr->Assign(forward_address);
  } else if (!collect_from_space_only_ && !immune_region_.ContainsObject(obj)) {
    BitmapSetSlowPathVisitor visitor(this);
    if (!mark_bitmap_->Set(obj, visitor)) {
      // This object was not previously marked.
      MarkStackPush(obj);
    }
  }
}

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_SEMI_SPACE_INL_H_
