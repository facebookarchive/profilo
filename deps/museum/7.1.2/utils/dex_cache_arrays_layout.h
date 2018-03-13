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

#ifndef ART_RUNTIME_UTILS_DEX_CACHE_ARRAYS_LAYOUT_H_
#define ART_RUNTIME_UTILS_DEX_CACHE_ARRAYS_LAYOUT_H_

#include "dex_file.h"

namespace art {

/**
 * @class DexCacheArraysLayout
 * @details This class provides the layout information for the type, method, field and
 * string arrays for a DexCache with a fixed arrays' layout (such as in the boot image),
 */
class DexCacheArraysLayout {
 public:
  // Construct an invalid layout.
  DexCacheArraysLayout()
      : /* types_offset_ is always 0u */
        pointer_size_(0u),
        methods_offset_(0u),
        strings_offset_(0u),
        fields_offset_(0u),
        size_(0u) {
  }

  // Construct a layout for a particular dex file header.
  DexCacheArraysLayout(size_t pointer_size, const DexFile::Header& header);

  // Construct a layout for a particular dex file.
  DexCacheArraysLayout(size_t pointer_size, const DexFile* dex_file);

  bool Valid() const {
    return Size() != 0u;
  }

  size_t Size() const {
    return size_;
  }

  size_t Alignment() const;

  size_t TypesOffset() const {
    return types_offset_;
  }

  size_t TypeOffset(uint32_t type_idx) const;

  size_t TypesSize(size_t num_elements) const;

  size_t TypesAlignment() const;

  size_t MethodsOffset() const {
    return methods_offset_;
  }

  size_t MethodOffset(uint32_t method_idx) const;

  size_t MethodsSize(size_t num_elements) const;

  size_t MethodsAlignment() const;

  size_t StringsOffset() const {
    return strings_offset_;
  }

  size_t StringOffset(uint32_t string_idx) const;

  size_t StringsSize(size_t num_elements) const;

  size_t StringsAlignment() const;

  size_t FieldsOffset() const {
    return fields_offset_;
  }

  size_t FieldOffset(uint32_t field_idx) const;

  size_t FieldsSize(size_t num_elements) const;

  size_t FieldsAlignment() const;

 private:
  static constexpr size_t types_offset_ = 0u;
  const size_t pointer_size_;  // Must be first for construction initialization order.
  const size_t methods_offset_;
  const size_t strings_offset_;
  const size_t fields_offset_;
  const size_t size_;

  static size_t Alignment(size_t pointer_size);

  static size_t ElementOffset(size_t element_size, uint32_t idx);

  static size_t ArraySize(size_t element_size, uint32_t num_elements);
};

}  // namespace art

#endif  // ART_RUNTIME_UTILS_DEX_CACHE_ARRAYS_LAYOUT_H_
