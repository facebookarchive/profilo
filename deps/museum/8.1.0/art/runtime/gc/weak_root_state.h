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

#ifndef ART_RUNTIME_GC_WEAK_ROOT_STATE_H_
#define ART_RUNTIME_GC_WEAK_ROOT_STATE_H_

#include <iosfwd>

namespace art {
namespace gc {

enum WeakRootState {
  // Can read or add weak roots.
  kWeakRootStateNormal,
  // Need to wait until we can read weak roots.
  kWeakRootStateNoReadsOrWrites,
  // Need to mark new weak roots to make sure they don't get swept.
  // kWeakRootStateMarkNewRoots is currently unused but I was planning on using to allow adding new
  // weak roots during the CMS reference processing phase.
  kWeakRootStateMarkNewRoots,
};

std::ostream& operator<<(std::ostream& os, const WeakRootState&);

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_WEAK_ROOT_STATE_H_
