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

#ifndef ART_RUNTIME_MIRROR_EXECUTABLE_H_
#define ART_RUNTIME_MIRROR_EXECUTABLE_H_

#include "accessible_object.h"
#include "gc_root.h"
#include "object.h"
#include "read_barrier_option.h"

namespace art {

struct ExecutableOffsets;
class ArtMethod;

namespace mirror {

// C++ mirror of java.lang.reflect.Executable.
class MANAGED Executable : public AccessibleObject {
 public:
  // Called from Constructor::CreateFromArtMethod, Method::CreateFromArtMethod.
  template <PointerSize kPointerSize, bool kTransactionActive>
  bool CreateFromArtMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  ArtMethod* GetArtMethod() REQUIRES_SHARED(Locks::mutator_lock_);
  // Only used by the image writer.
  template <bool kTransactionActive = false>
  void SetArtMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Class* GetDeclaringClass() REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  uint16_t has_real_parameter_data_;
  HeapReference<mirror::Class> declaring_class_;
  HeapReference<mirror::Class> declaring_class_of_overridden_method_;
  HeapReference<mirror::Array> parameters_;
  uint64_t art_method_;
  uint32_t access_flags_;
  uint32_t dex_method_index_;

  static MemberOffset ArtMethodOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Executable, art_method_));
  }
  static MemberOffset DeclaringClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Executable, declaring_class_));
  }
  static MemberOffset DeclaringClassOfOverriddenMethodOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Executable, declaring_class_of_overridden_method_));
  }
  static MemberOffset AccessFlagsOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Executable, access_flags_));
  }
  static MemberOffset DexMethodIndexOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Executable, dex_method_index_));
  }

  friend struct art::ExecutableOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Executable);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_EXECUTABLE_H_
