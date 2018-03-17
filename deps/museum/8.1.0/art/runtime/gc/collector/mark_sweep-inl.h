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

#ifndef ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_INL_H_
#define ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_INL_H_

#include "mark_sweep.h"

#include "gc/heap.h"
#include "mirror/class-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/reference.h"

namespace art {
namespace gc {
namespace collector {

template<typename MarkVisitor, typename ReferenceVisitor>
inline void MarkSweep::ScanObjectVisit(mirror::Object* obj,
                                       const MarkVisitor& visitor,
                                       const ReferenceVisitor& ref_visitor) {
  DCHECK(IsMarked(obj)) << "Scanning unmarked object " << obj << "\n" << heap_->DumpSpaces();
  obj->VisitReferences(visitor, ref_visitor);
  if (kCountScannedTypes) {
    mirror::Class* klass = obj->GetClass<kVerifyNone>();
    uint32_t class_flags = klass->GetClassFlags();
    if ((class_flags & mirror::kClassFlagNoReferenceFields) != 0) {
      ++no_reference_class_count_;
    } else if (class_flags == mirror::kClassFlagNormal) {
      ++normal_count_;
    } else if (class_flags == mirror::kClassFlagObjectArray) {
      ++object_array_count_;
    } else if (class_flags == mirror::kClassFlagClass) {
      ++class_count_;
    } else if ((class_flags & mirror::kClassFlagReference) != 0) {
      ++reference_count_;
    } else {
      ++other_count_;
    }
  }
}

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_SWEEP_INL_H_
