/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_FIELD_HELPER_H_
#define ART_RUNTIME_FIELD_HELPER_H_

#include "base/macros.h"
#include "handle.h"
#include "mirror/art_field.h"

namespace art {

class FieldHelper {
 public:
  explicit FieldHelper(Handle<mirror::ArtField> f) : field_(f) {}

  void ChangeField(mirror::ArtField* new_f) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(new_f != nullptr);
    field_.Assign(new_f);
  }

  mirror::ArtField* GetField() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return field_.Get();
  }

  mirror::Class* GetType(bool resolve = true) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // The returned const char* is only guaranteed to be valid for the lifetime of the FieldHelper.
  // If you need it longer, copy it into a std::string.
  const char* GetDeclaringClassDescriptor() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  Handle<mirror::ArtField> field_;
  std::string declaring_class_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(FieldHelper);
};

}  // namespace art

#endif  // ART_RUNTIME_FIELD_HELPER_H_
