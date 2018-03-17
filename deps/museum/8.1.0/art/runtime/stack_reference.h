/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_STACK_REFERENCE_H_
#define ART_RUNTIME_STACK_REFERENCE_H_

#include "base/macros.h"
#include "mirror/object_reference.h"

namespace art {

// A reference from the shadow stack to a MirrorType object within the Java heap.
template<class MirrorType>
class PACKED(4) StackReference : public mirror::CompressedReference<MirrorType> {
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_REFERENCE_H_
