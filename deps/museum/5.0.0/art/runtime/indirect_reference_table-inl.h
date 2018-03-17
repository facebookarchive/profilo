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

#ifndef ART_RUNTIME_INDIRECT_REFERENCE_TABLE_INL_H_
#define ART_RUNTIME_INDIRECT_REFERENCE_TABLE_INL_H_

#include "indirect_reference_table.h"

#include "verify_object-inl.h"

namespace art {
namespace mirror {
class Object;
}  // namespace mirror

// Verifies that the indirect table lookup is valid.
// Returns "false" if something looks bad.
inline bool IndirectReferenceTable::GetChecked(IndirectRef iref) const {
  if (UNLIKELY(iref == nullptr)) {
    LOG(WARNING) << "Attempt to look up NULL " << kind_;
    return false;
  }
  if (UNLIKELY(GetIndirectRefKind(iref) == kHandleScopeOrInvalid)) {
    LOG(ERROR) << "JNI ERROR (app bug): invalid " << kind_ << " " << iref;
    AbortIfNoCheckJNI();
    return false;
  }
  const int topIndex = segment_state_.parts.topIndex;
  int idx = ExtractIndex(iref);
  if (UNLIKELY(idx >= topIndex)) {
    LOG(ERROR) << "JNI ERROR (app bug): accessed stale " << kind_ << " "
               << iref << " (index " << idx << " in a table of size " << topIndex << ")";
    AbortIfNoCheckJNI();
    return false;
  }
  if (UNLIKELY(table_[idx].IsNull())) {
    LOG(ERROR) << "JNI ERROR (app bug): accessed deleted " << kind_ << " " << iref;
    AbortIfNoCheckJNI();
    return false;
  }
  if (UNLIKELY(!CheckEntry("use", iref, idx))) {
    return false;
  }
  return true;
}

// Make sure that the entry at "idx" is correctly paired with "iref".
inline bool IndirectReferenceTable::CheckEntry(const char* what, IndirectRef iref, int idx) const {
  IndirectRef checkRef = ToIndirectRef(idx);
  if (UNLIKELY(checkRef != iref)) {
    LOG(ERROR) << "JNI ERROR (app bug): attempt to " << what
               << " stale " << kind_ << " " << iref
               << " (should be " << checkRef << ")";
    AbortIfNoCheckJNI();
    return false;
  }
  return true;
}

template<ReadBarrierOption kReadBarrierOption>
inline mirror::Object* IndirectReferenceTable::Get(IndirectRef iref) const {
  if (!GetChecked(iref)) {
    return kInvalidIndirectRefObject;
  }
  uint32_t idx = ExtractIndex(iref);
  mirror::Object* obj = table_[idx].Read<kWithoutReadBarrier>();
  if (LIKELY(obj != kClearedJniWeakGlobal)) {
    // The read barrier or VerifyObject won't handle kClearedJniWeakGlobal.
    obj = table_[idx].Read();
    VerifyObject(obj);
  }
  return obj;
}

}  // namespace art

#endif  // ART_RUNTIME_INDIRECT_REFERENCE_TABLE_INL_H_
