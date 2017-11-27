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

#ifndef ART_RUNTIME_LAMBDA_CLOSURE_BUILDER_INL_H_
#define ART_RUNTIME_LAMBDA_CLOSURE_BUILDER_INL_H_

#include "lambda/closure_builder.h"
#include <string.h>

namespace art {
namespace lambda {

template <typename T, ClosureBuilder::ShortyTypeEnum kShortyType>
void ClosureBuilder::CaptureVariablePrimitive(T value) {
  static_assert(ShortyFieldTypeTraits::IsPrimitiveType<T>(), "T must be a primitive type");
  const size_t type_size = ShortyFieldType(kShortyType).GetStaticSize();
  DCHECK_EQ(type_size, sizeof(T));

  // Copy the data while retaining the bit pattern. Strict-aliasing safe.
  ShortyFieldTypeTraits::MaxType value_storage = 0;
  memcpy(&value_storage, &value, sizeof(T));

  values_.push_back(value_storage);
  size_ += sizeof(T);

  shorty_types_ += kShortyType;
}

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_CLOSURE_BUILDER_INL_H_
