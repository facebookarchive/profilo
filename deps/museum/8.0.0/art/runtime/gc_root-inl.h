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

#ifndef ART_RUNTIME_GC_ROOT_INL_H_
#define ART_RUNTIME_GC_ROOT_INL_H_

#include "gc_root.h"

#include <sstream>

#include "obj_ptr-inl.h"
#include "read_barrier-inl.h"

namespace art {

template<class MirrorType>
template<ReadBarrierOption kReadBarrierOption>
inline MirrorType* GcRoot<MirrorType>::Read(GcRootSource* gc_root_source) const {
  return down_cast<MirrorType*>(
      ReadBarrier::BarrierForRoot<mirror::Object, kReadBarrierOption>(&root_, gc_root_source));
}

template<class MirrorType>
inline GcRoot<MirrorType>::GcRoot(MirrorType* ref)
    : root_(mirror::CompressedReference<mirror::Object>::FromMirrorPtr(ref)) { }

template<class MirrorType>
inline GcRoot<MirrorType>::GcRoot(ObjPtr<MirrorType> ref)
    : GcRoot(ref.Ptr()) { }

inline std::string RootInfo::ToString() const {
  std::ostringstream oss;
  Describe(oss);
  return oss.str();
}

}  // namespace art
#endif  // ART_RUNTIME_GC_ROOT_INL_H_
