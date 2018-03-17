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

#ifndef ART_RUNTIME_STRING_REFERENCE_H_
#define ART_RUNTIME_STRING_REFERENCE_H_

#include <stdint.h>

#include "base/logging.h"
#include "dex_file-inl.h"
#include "dex_file_types.h"
#include "utf-inl.h"

namespace art {

// A string is located by its DexFile and the string_ids_ table index into that DexFile.
struct StringReference {
  StringReference(const DexFile* file, dex::StringIndex index)
      : dex_file(file), string_index(index) { }

  const char* GetStringData() const {
    return dex_file->GetStringData(dex_file->GetStringId(string_index));
  }

  const DexFile* dex_file;
  dex::StringIndex string_index;
};

// Compare only the reference and not the string contents.
struct StringReferenceComparator {
  bool operator()(const StringReference& a, const StringReference& b) {
    if (a.dex_file != b.dex_file) {
      return a.dex_file < b.dex_file;
    }
    return a.string_index < b.string_index;
  }
};

// Compare the actual referenced string values. Used for string reference deduplication.
struct StringReferenceValueComparator {
  bool operator()(StringReference sr1, StringReference sr2) const {
    // Note that we want to deduplicate identical strings even if they are referenced
    // by different dex files, so we need some (any) total ordering of strings, rather
    // than references. However, the references should usually be from the same dex file,
    // so we choose the dex file string ordering so that we can simply compare indexes
    // and avoid the costly string comparison in the most common case.
    if (sr1.dex_file == sr2.dex_file) {
      // Use the string order enforced by the dex file verifier.
      DCHECK_EQ(
          sr1.string_index < sr2.string_index,
          CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(sr1.GetStringData(),
                                                                  sr2.GetStringData()) < 0);
      return sr1.string_index < sr2.string_index;
    } else {
      // Cannot compare indexes, so do the string comparison.
      return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(sr1.GetStringData(),
                                                                     sr2.GetStringData()) < 0;
    }
  }
};

}  // namespace art

#endif  // ART_RUNTIME_STRING_REFERENCE_H_
