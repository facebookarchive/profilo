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
#ifndef ART_RUNTIME_LAMBDA_CLOSURE_BUILDER_H_
#define ART_RUNTIME_LAMBDA_CLOSURE_BUILDER_H_

#include "base/macros.h"
#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "base/value_object.h"
#include "lambda/shorty_field_type.h"

#include <stdint.h>
#include <vector>

namespace art {
class ArtMethod;  // forward declaration

namespace mirror {
class Object;  // forward declaration
}  // namespace mirror

namespace lambda {
class ArtLambdaMethod;  // forward declaration

// Build a closure by capturing variables one at a time.
// When all variables have been marked captured, the closure can be created in-place into
// a target memory address.
//
// The mutator lock must be held for the duration of the lifetime of this object,
// since it needs to temporarily store heap references into an internal list.
class ClosureBuilder {
 public:
  using ShortyTypeEnum = decltype(ShortyFieldType::kByte);

  // Mark this primitive value to be captured as the specified type.
  template <typename T, ShortyTypeEnum kShortyType = ShortyFieldTypeSelectEnum<T>::value>
  void CaptureVariablePrimitive(T value);

  // Mark this object reference to be captured.
  void CaptureVariableObject(mirror::Object* object) SHARED_REQUIRES(Locks::mutator_lock_);

  // Mark this lambda closure to be captured.
  void CaptureVariableLambda(Closure* closure);

  // Get the size (in bytes) of the closure.
  // This size is used to be able to allocate memory large enough to write the closure into.
  // Call 'CreateInPlace' to actually write the closure out.
  size_t GetSize() const;

  // Returns how many variables have been captured so far.
  size_t GetCaptureCount() const;

  // Get the list of captured variables' shorty field types.
  const std::string& GetCapturedVariableShortyTypes() const;

  // Creates a closure in-place and writes out the data into 'memory'.
  // Memory must be at least 'GetSize' bytes large.
  // All previously marked data to be captured is now written out.
  Closure* CreateInPlace(void* memory, ArtLambdaMethod* target_method) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Locks need to be held for entire lifetime of ClosureBuilder.
  ClosureBuilder() SHARED_REQUIRES(Locks::mutator_lock_)
  {}

  // Locks need to be held for entire lifetime of ClosureBuilder.
  ~ClosureBuilder() SHARED_REQUIRES(Locks::mutator_lock_)
  {}

 private:
  // Initial size a closure starts out before any variables are written.
  // Header size only.
  static constexpr size_t kInitialSize = sizeof(ArtLambdaMethod*);

  // Write a Closure's variables field from the captured variables.
  // variables_size specified in bytes, and only includes enough room to write variables into.
  // Returns the calculated actual size of the closure.
  size_t WriteValues(ArtLambdaMethod* target_method,
                     uint8_t variables[],
                     size_t header_size,
                     size_t variables_size) const SHARED_REQUIRES(Locks::mutator_lock_);

  size_t size_ = kInitialSize;
  bool is_dynamic_size_ = false;
  std::vector<ShortyFieldTypeTraits::MaxType> values_;
  std::string shorty_types_;
};

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_CLOSURE_BUILDER_H_
