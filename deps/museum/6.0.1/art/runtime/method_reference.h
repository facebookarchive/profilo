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

#ifndef ART_RUNTIME_METHOD_REFERENCE_H_
#define ART_RUNTIME_METHOD_REFERENCE_H_

#include <stdint.h>

namespace art {

class DexFile;

// A method is uniquely located by its DexFile and the method_ids_ table index into that DexFile
struct MethodReference {
  MethodReference(const DexFile* file, uint32_t index) : dex_file(file), dex_method_index(index) {
  }
  const DexFile* dex_file;
  uint32_t dex_method_index;
};

struct MethodReferenceComparator {
  bool operator()(MethodReference mr1, MethodReference mr2) const {
    if (mr1.dex_file == mr2.dex_file) {
      return mr1.dex_method_index < mr2.dex_method_index;
    } else {
      return mr1.dex_file < mr2.dex_file;
    }
  }
};

}  // namespace art

#endif  // ART_RUNTIME_METHOD_REFERENCE_H_
