/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_CLASS_LOADER_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_LOADER_INL_H_

#include "class_loader.h"

#include "base/mutex-inl.h"
#include "class_table-inl.h"

namespace art {
namespace mirror {

template <bool kVisitClasses,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor>
inline void ClassLoader::VisitReferences(mirror::Class* klass, const Visitor& visitor) {
  // Visit instance fields first.
  VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
  if (kVisitClasses) {
    // Visit classes loaded after.
    ClassTable* const class_table = GetClassTable();
    if (class_table != nullptr) {
      class_table->VisitRoots(visitor);
    }
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_LOADER_INL_H_
