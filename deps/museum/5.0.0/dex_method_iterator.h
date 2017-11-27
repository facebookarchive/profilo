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

#ifndef ART_RUNTIME_DEX_METHOD_ITERATOR_H_
#define ART_RUNTIME_DEX_METHOD_ITERATOR_H_

#include <vector>

#include "dex_file.h"

namespace art {

class DexMethodIterator {
 public:
  explicit DexMethodIterator(const std::vector<const DexFile*>& dex_files)
      : dex_files_(dex_files),
        found_next_(false),
        dex_file_index_(0),
        class_def_index_(0),
        class_def_(NULL),
        class_data_(NULL),
        direct_method_(false) {
    CHECK_NE(0U, dex_files_.size());
  }

  bool HasNext() {
    if (found_next_) {
      return true;
    }
    while (true) {
      // End of DexFiles, we are done.
      if (dex_file_index_ == dex_files_.size()) {
        return false;
      }
      if (class_def_index_ == GetDexFileInternal().NumClassDefs()) {
        // End of this DexFile, advance and retry.
        class_def_index_ = 0;
        dex_file_index_++;
        continue;
      }
      if (class_def_ == NULL) {
        class_def_ = &GetDexFileInternal().GetClassDef(class_def_index_);
      }
      if (class_data_ == NULL) {
        class_data_ = GetDexFileInternal().GetClassData(*class_def_);
        if (class_data_ == NULL) {
          // empty class, such as a marker interface
          // End of this class, advance and retry.
          class_def_ = NULL;
          class_def_index_++;
          continue;
        }
      }
      if (it_.get() == NULL) {
        it_.reset(new ClassDataItemIterator(GetDexFileInternal(), class_data_));
        // Skip fields
        while (GetIterator().HasNextStaticField()) {
          GetIterator().Next();
        }
        while (GetIterator().HasNextInstanceField()) {
          GetIterator().Next();
        }
        direct_method_ = true;
      }
      if (direct_method_ && GetIterator().HasNextDirectMethod()) {
        // Found method
        found_next_ = true;
        return true;
      }
      direct_method_ = false;
      if (GetIterator().HasNextVirtualMethod()) {
        // Found method
        found_next_ = true;
        return true;
      }
      // End of this class, advance and retry.
      DCHECK(!GetIterator().HasNext());
      it_.reset(NULL);
      class_data_ = NULL;
      class_def_ = NULL;
      class_def_index_++;
    }
  }

  void Next() {
    found_next_ = false;
    if (it_.get() != NULL) {
      // Advance to next method if we currently are looking at a class.
      GetIterator().Next();
    }
  }

  const DexFile& GetDexFile() {
    CHECK(HasNext());
    return GetDexFileInternal();
  }

  uint32_t GetMemberIndex() {
    CHECK(HasNext());
    return GetIterator().GetMemberIndex();
  }

  InvokeType GetInvokeType() {
    CHECK(HasNext());
    CHECK(class_def_ != NULL);
    return GetIterator().GetMethodInvokeType(*class_def_);
  }

 private:
  ClassDataItemIterator& GetIterator() const {
    CHECK(it_.get() != NULL);
    return *it_.get();
  }

  const DexFile& GetDexFileInternal() const {
    CHECK_LT(dex_file_index_, dex_files_.size());
    const DexFile* dex_file = dex_files_[dex_file_index_];
    CHECK(dex_file != NULL);
    return *dex_file;
  }

  const std::vector<const DexFile*>& dex_files_;

  bool found_next_;

  uint32_t dex_file_index_;
  uint32_t class_def_index_;
  const DexFile::ClassDef* class_def_;
  const byte* class_data_;
  std::unique_ptr<ClassDataItemIterator> it_;
  bool direct_method_;
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_METHOD_ITERATOR_H_
