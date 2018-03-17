/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_NON_DEBUGGABLE_CLASSES_H_
#define ART_RUNTIME_NON_DEBUGGABLE_CLASSES_H_

#include <vector>

#include "base/mutex.h"
#include "jni.h"

namespace art {

struct NonDebuggableClasses {
 public:
  static const std::vector<jclass>& GetNonDebuggableClasses() {
    return non_debuggable_classes;
  }

  static void AddNonDebuggableClass(jclass klass)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static std::vector<jclass> non_debuggable_classes;
};

}  // namespace art

#endif  // ART_RUNTIME_NON_DEBUGGABLE_CLASSES_H_
