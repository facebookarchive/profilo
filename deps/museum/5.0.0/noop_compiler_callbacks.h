/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_NOOP_COMPILER_CALLBACKS_H_
#define ART_RUNTIME_NOOP_COMPILER_CALLBACKS_H_

#include "compiler_callbacks.h"

namespace art {

class NoopCompilerCallbacks FINAL : public CompilerCallbacks {
 public:
  NoopCompilerCallbacks() {}
  ~NoopCompilerCallbacks() {}

  bool MethodVerified(verifier::MethodVerifier* verifier) OVERRIDE {
    return true;
  }

  void ClassRejected(ClassReference ref) OVERRIDE {}

  // This is only used by compilers which need to be able to run without relocation even when it
  // would normally be enabled. For example the patchoat executable, and dex2oat --image, both need
  // to disable the relocation since both deal with writing out the images directly.
  bool IsRelocationPossible() OVERRIDE { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopCompilerCallbacks);
};

}  // namespace art

#endif  // ART_RUNTIME_NOOP_COMPILER_CALLBACKS_H_
