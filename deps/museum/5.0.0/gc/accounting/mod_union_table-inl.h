/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_INL_H_

#include "mod_union_table.h"

#include "gc/space/space.h"

namespace art {
namespace gc {
namespace accounting {

// A mod-union table to record image references to the Zygote and alloc space.
class ModUnionTableToZygoteAllocspace : public ModUnionTableReferenceCache {
 public:
  explicit ModUnionTableToZygoteAllocspace(const std::string& name, Heap* heap,
                                           space::ContinuousSpace* space)
      : ModUnionTableReferenceCache(name, heap, space) {}

  bool ShouldAddReference(const mirror::Object* ref) const OVERRIDE ALWAYS_INLINE {
    return !space_->HasAddress(ref);
  }
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_INL_H_
