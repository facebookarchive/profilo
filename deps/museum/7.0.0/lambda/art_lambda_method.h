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
#ifndef ART_RUNTIME_LAMBDA_ART_LAMBDA_METHOD_H_
#define ART_RUNTIME_LAMBDA_ART_LAMBDA_METHOD_H_

#include "base/macros.h"
#include "art_method.h"

#include <stdint.h>

namespace art {
namespace lambda {

class ArtLambdaMethod {
 public:
  // Construct an art lambda method.
  // The target method is the one invoked by invoke-lambda.
  // The type descriptor describes the types of variables captured, e.g. "ZFLObject;\FI;[Z"
  // The shorty drops the object name and treats arrays as objects, e.g. "ZFL\L"
  // Innate lambda means that the lambda was originally created via invoke-lambda.
  // -- Non-innate lambdas (learned lambdas) come from a regular class that was boxed to lambda.
  // (Ownership of strings is retained by the caller and the lifetime should exceed this class).
  ArtLambdaMethod(ArtMethod* target_method,
                  const char* captured_variables_type_descriptor,
                  const char* captured_variables_shorty,
                  bool innate_lambda = true);

  // Get the target method for this lambda that would be used by the invoke-lambda dex instruction.
  ArtMethod* GetArtMethod() const {
    return method_;
  }

  // Get the compile-time size of lambda closures for this method in bytes.
  // This is circular (that is, it includes the size of the ArtLambdaMethod pointer).
  // One should also check if the size is dynamic since nested lambdas have a runtime size.
  size_t GetStaticClosureSize() const {
    return closure_size_;
  }

  // Get the type descriptor for the list of captured variables.
  // e.g. "ZFLObject;\FI;[Z" means a captured int, float, class Object, lambda FI, array of ints
  const char* GetCapturedVariablesTypeDescriptor() const {
    return captured_variables_type_descriptor_;
  }

  // Get the shorty 'field' type descriptor list of captured variables.
  // This follows the same rules as a string of ShortyFieldType in the dex specification.
  // Every captured variable is represented by exactly one character.
  // - Objects become 'L'.
  // - Arrays become 'L'.
  // - Lambdas become '\'.
  const char* GetCapturedVariablesShortyTypeDescriptor() const {
    return captured_variables_shorty_;
  }

  // Will the size of this lambda change at runtime?
  // Only returns true if there is a nested lambda that we can't determine statically the size of.
  bool IsDynamicSize() const {
    return dynamic_size_;
  }

  // Will the size of this lambda always be constant at runtime?
  // This generally means there's no nested lambdas, or we were able to successfully determine
  // their size statically at compile time.
  bool IsStaticSize() const {
    return !IsDynamicSize();
  }
  // Is this a lambda that was originally created via invoke-lambda?
  // -- Non-innate lambdas (learned lambdas) come from a regular class that was boxed to lambda.
  bool IsInnateLambda() const {
    return innate_lambda_;
  }

  // How many variables were captured?
  // (Each nested lambda counts as 1 captured var regardless of how many captures it itself has).
  size_t GetNumberOfCapturedVariables() const {
    return strlen(captured_variables_shorty_);
  }

 private:
  // TODO: ArtMethod, or at least the entry points should be inlined into this struct
  // to avoid an extra indirect load when doing invokes.
  // Target method that invoke-lambda will jump to.
  ArtMethod* method_;
  // How big the closure is (in bytes). Only includes the constant size.
  size_t closure_size_;
  // The type descriptor for the captured variables, e.g. "IS" for [int, short]
  const char* captured_variables_type_descriptor_;
  // The shorty type descriptor for captured vars, (e.g. using 'L' instead of 'LObject;')
  const char* captured_variables_shorty_;
  // Whether or not the size is dynamic. If it is, copiers need to read the Closure size at runtime.
  bool dynamic_size_;
  // True if this lambda was originally made with create-lambda,
  // false if it came from a class instance (through new-instance and then unbox-lambda).
  bool innate_lambda_;

  DISALLOW_COPY_AND_ASSIGN(ArtLambdaMethod);
};

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_ART_LAMBDA_METHOD_H_
