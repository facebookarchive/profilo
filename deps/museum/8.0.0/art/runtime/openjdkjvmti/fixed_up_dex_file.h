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

#ifndef ART_RUNTIME_OPENJDKJVMTI_FIXED_UP_DEX_FILE_H_
#define ART_RUNTIME_OPENJDKJVMTI_FIXED_UP_DEX_FILE_H_

#include <memory>
#include <vector>

#include "jni.h"
#include "jvmti.h"
#include "base/mutex.h"
#include "dex_file.h"

namespace openjdkjvmti {

// A holder for a DexFile that has been 'fixed up' to ensure it is fully compliant with the
// published standard (no internal/quick opcodes, all fields are the defined values, etc). This is
// used to ensure that agents get a consistent dex file regardless of what version of android they
// are running on.
class FixedUpDexFile {
 public:
  static std::unique_ptr<FixedUpDexFile> Create(const art::DexFile& original)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  const art::DexFile& GetDexFile() {
    return *dex_file_;
  }

  const unsigned char* Begin() {
    return data_.data();
  }

  size_t Size() {
    return data_.size();
  }

 private:
  explicit FixedUpDexFile(std::unique_ptr<const art::DexFile> fixed_up_dex_file,
                          std::vector<unsigned char> data)
      : dex_file_(std::move(fixed_up_dex_file)),
        data_(std::move(data)) {}

  // the fixed up DexFile
  std::unique_ptr<const art::DexFile> dex_file_;
  // The backing data for dex_file_.
  const std::vector<unsigned char> data_;

  DISALLOW_COPY_AND_ASSIGN(FixedUpDexFile);
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_FIXED_UP_DEX_FILE_H_
