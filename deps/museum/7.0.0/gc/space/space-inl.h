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

#ifndef ART_RUNTIME_GC_SPACE_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_SPACE_INL_H_

#include "space.h"

#include "base/casts.h"
#include "dlmalloc_space.h"
#include "image_space.h"
#include "large_object_space.h"

namespace art {
namespace gc {
namespace space {

inline ImageSpace* Space::AsImageSpace() {
  DCHECK(IsImageSpace());
  return down_cast<ImageSpace*>(down_cast<MemMapSpace*>(this));
}

inline MallocSpace* Space::AsMallocSpace() {
  DCHECK(IsMallocSpace());
  DCHECK(IsDlMallocSpace() || IsRosAllocSpace());
  return down_cast<MallocSpace*>(down_cast<MemMapSpace*>(this));
}

inline LargeObjectSpace* Space::AsLargeObjectSpace() {
  DCHECK(IsLargeObjectSpace());
  return down_cast<LargeObjectSpace*>(this);
}

inline ContinuousSpace* Space::AsContinuousSpace() {
  DCHECK(IsContinuousSpace());
  return down_cast<ContinuousSpace*>(this);
}

inline DiscontinuousSpace* Space::AsDiscontinuousSpace() {
  DCHECK(IsDiscontinuousSpace());
  return down_cast<DiscontinuousSpace*>(this);
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_SPACE_INL_H_
