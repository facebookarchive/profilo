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

#ifndef ART_RUNTIME_AOT_CLASS_LINKER_H_
#define ART_RUNTIME_AOT_CLASS_LINKER_H_

#include "class_linker.h"

namespace art {

// AotClassLinker is only used for AOT compiler, which includes some logic for class initialization
// which will only be used in pre-compilation.
class AotClassLinker : public ClassLinker {
 public:
  explicit AotClassLinker(InternTable *intern_table);
  ~AotClassLinker();

 protected:
  // Overridden version of PerformClassVerification allows skipping verification if the class was
  // previously verified but unloaded.
  verifier::FailureKind PerformClassVerification(Thread* self,
                                                 Handle<mirror::Class> klass,
                                                 verifier::HardFailLogMode log_level,
                                                 std::string* error_msg)
      OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_AOT_CLASS_LINKER_H_
