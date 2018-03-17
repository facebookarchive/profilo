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

#ifndef ART_RUNTIME_MIRROR_IFTABLE_INL_H_
#define ART_RUNTIME_MIRROR_IFTABLE_INL_H_

#include "iftable.h"

namespace art {
namespace mirror {

inline void IfTable::SetInterface(int32_t i, Class* interface) {
  DCHECK(interface != NULL);
  DCHECK(interface->IsInterface());
  const size_t idx = i * kMax + kInterface;
  DCHECK_EQ(Get(idx), static_cast<Object*>(nullptr));
  Set<false>(idx, interface);
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_IFTABLE_INL_H_
