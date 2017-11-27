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

#ifndef ART_RUNTIME_COMPILER_CALLBACKS_H_
#define ART_RUNTIME_COMPILER_CALLBACKS_H_

#include "base/mutex.h"
#include "class_reference.h"

namespace art {

namespace verifier {

class MethodVerifier;

}  // namespace verifier

class CompilerCallbacks {
 public:
  enum class CallbackMode {  // private
    kCompileBootImage,
    kCompileApp
  };

  virtual ~CompilerCallbacks() { }

  virtual void MethodVerified(verifier::MethodVerifier* verifier)
      SHARED_REQUIRES(Locks::mutator_lock_) = 0;
  virtual void ClassRejected(ClassReference ref) = 0;

  // Return true if we should attempt to relocate to a random base address if we have not already
  // done so. Return false if relocating in this way would be problematic.
  virtual bool IsRelocationPossible() = 0;

  bool IsBootImage() {
    return mode_ == CallbackMode::kCompileBootImage;
  }

 protected:
  explicit CompilerCallbacks(CallbackMode mode) : mode_(mode) { }

 private:
  // Whether the compiler is creating a boot image.
  const CallbackMode mode_;
};

}  // namespace art

#endif  // ART_RUNTIME_COMPILER_CALLBACKS_H_
