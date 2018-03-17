/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_DEFINITION_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_DEFINITION_H_

#include "art_jvmti.h"

namespace openjdkjvmti {

// A struct that stores data needed for redefining/transforming classes. This structure should only
// even be accessed from a single thread and must not survive past the completion of the
// redefinition/retransformation function that created it.
class ArtClassDefinition {
 public:
  ArtClassDefinition()
      : klass_(nullptr),
        loader_(nullptr),
        name_(),
        protection_domain_(nullptr),
        dex_len_(0),
        dex_data_(nullptr),
        original_dex_file_memory_(nullptr),
        original_dex_file_(),
        redefined_(false) {}

  jvmtiError Init(ArtJvmTiEnv* env, jclass klass);
  jvmtiError Init(ArtJvmTiEnv* env, const jvmtiClassDefinition& def);

  ArtClassDefinition(ArtClassDefinition&& o) = default;
  ArtClassDefinition& operator=(ArtClassDefinition&& o) = default;

  void SetNewDexData(ArtJvmTiEnv* env, jint new_dex_len, unsigned char* new_dex_data) {
    DCHECK(IsInitialized());
    if (new_dex_data == nullptr) {
      return;
    } else if (new_dex_data != dex_data_.get() || new_dex_len != dex_len_) {
      dex_len_ = new_dex_len;
      dex_data_ = MakeJvmtiUniquePtr(env, new_dex_data);
    }
  }

  art::ArraySlice<const unsigned char> GetNewOriginalDexFile() const {
    DCHECK(IsInitialized());
    if (redefined_) {
      return original_dex_file_;
    } else {
      return art::ArraySlice<const unsigned char>();
    }
  }

  bool IsModified() const;

  bool IsInitialized() const {
    return klass_ != nullptr;
  }

  jclass GetClass() const {
    DCHECK(IsInitialized());
    return klass_;
  }

  jobject GetLoader() const {
    DCHECK(IsInitialized());
    return loader_;
  }

  const std::string& GetName() const {
    DCHECK(IsInitialized());
    return name_;
  }

  jobject GetProtectionDomain() const {
    DCHECK(IsInitialized());
    return protection_domain_;
  }

  art::ArraySlice<const unsigned char> GetDexData() const {
    DCHECK(IsInitialized());
    return art::ArraySlice<const unsigned char>(dex_data_.get(), dex_len_);
  }

 private:
  jvmtiError InitCommon(ArtJvmTiEnv* env, jclass klass);

  jclass klass_;
  jobject loader_;
  std::string name_;
  jobject protection_domain_;
  jint dex_len_;
  JvmtiUniquePtr<unsigned char> dex_data_;
  JvmtiUniquePtr<unsigned char> original_dex_file_memory_;
  art::ArraySlice<const unsigned char> original_dex_file_;
  bool redefined_;

  DISALLOW_COPY_AND_ASSIGN(ArtClassDefinition);
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_DEFINITION_H_
