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

#ifndef ART_RUNTIME_UTILS_DEX_CACHE_ARRAYS_LAYOUT_INL_H_
#define ART_RUNTIME_UTILS_DEX_CACHE_ARRAYS_LAYOUT_INL_H_

#include "dex_cache_arrays_layout.h"

#include "base/bit_utils.h"
#include "base/logging.h"
#include "gc_root.h"
#include "globals.h"
#include "primitive.h"

namespace art {

inline DexCacheArraysLayout::DexCacheArraysLayout(size_t pointer_size,
                                                  const DexFile::Header& header)
    : pointer_size_(pointer_size),
      /* types_offset_ is always 0u, so it's constexpr */
      methods_offset_(
          RoundUp(types_offset_ + TypesSize(header.type_ids_size_), MethodsAlignment())),
      strings_offset_(
          RoundUp(methods_offset_ + MethodsSize(header.method_ids_size_), StringsAlignment())),
      fields_offset_(
          RoundUp(strings_offset_ + StringsSize(header.string_ids_size_), FieldsAlignment())),
      size_(
          RoundUp(fields_offset_ + FieldsSize(header.field_ids_size_), Alignment())) {
  DCHECK(ValidPointerSize(pointer_size)) << pointer_size;
}

inline DexCacheArraysLayout::DexCacheArraysLayout(size_t pointer_size, const DexFile* dex_file)
    : DexCacheArraysLayout(pointer_size, dex_file->GetHeader()) {
}

inline size_t DexCacheArraysLayout::Alignment() const {
  // GcRoot<> alignment is 4, i.e. lower than or equal to the pointer alignment.
  static_assert(alignof(GcRoot<mirror::Class>) == 4, "Expecting alignof(GcRoot<>) == 4");
  static_assert(alignof(GcRoot<mirror::String>) == 4, "Expecting alignof(GcRoot<>) == 4");
  DCHECK(pointer_size_ == 4u || pointer_size_ == 8u);
  // Pointer alignment is the same as pointer size.
  return pointer_size_;
}

inline size_t DexCacheArraysLayout::TypeOffset(uint32_t type_idx) const {
  return types_offset_ + ElementOffset(sizeof(GcRoot<mirror::Class>), type_idx);
}

inline size_t DexCacheArraysLayout::TypesSize(size_t num_elements) const {
  // App image patching relies on having enough room for a forwarding pointer in the types array.
  // See FixupArtMethodArrayVisitor and ClassLinker::AddImageSpace.
  return std::max(ArraySize(sizeof(GcRoot<mirror::Class>), num_elements), pointer_size_);
}

inline size_t DexCacheArraysLayout::TypesAlignment() const {
  return alignof(GcRoot<mirror::Class>);
}

inline size_t DexCacheArraysLayout::MethodOffset(uint32_t method_idx) const {
  return methods_offset_ + ElementOffset(pointer_size_, method_idx);
}

inline size_t DexCacheArraysLayout::MethodsSize(size_t num_elements) const {
  // App image patching relies on having enough room for a forwarding pointer in the methods array.
  return std::max(ArraySize(pointer_size_, num_elements), pointer_size_);
}

inline size_t DexCacheArraysLayout::MethodsAlignment() const {
  return pointer_size_;
}

inline size_t DexCacheArraysLayout::StringOffset(uint32_t string_idx) const {
  return strings_offset_ + ElementOffset(sizeof(GcRoot<mirror::String>), string_idx);
}

inline size_t DexCacheArraysLayout::StringsSize(size_t num_elements) const {
  return ArraySize(sizeof(GcRoot<mirror::String>), num_elements);
}

inline size_t DexCacheArraysLayout::StringsAlignment() const {
  return alignof(GcRoot<mirror::String>);
}

inline size_t DexCacheArraysLayout::FieldOffset(uint32_t field_idx) const {
  return fields_offset_ + ElementOffset(pointer_size_, field_idx);
}

inline size_t DexCacheArraysLayout::FieldsSize(size_t num_elements) const {
  return ArraySize(pointer_size_, num_elements);
}

inline size_t DexCacheArraysLayout::FieldsAlignment() const {
  return pointer_size_;
}

inline size_t DexCacheArraysLayout::ElementOffset(size_t element_size, uint32_t idx) {
  return element_size * idx;
}

inline size_t DexCacheArraysLayout::ArraySize(size_t element_size, uint32_t num_elements) {
  return element_size * num_elements;
}

}  // namespace art

#endif  // ART_RUNTIME_UTILS_DEX_CACHE_ARRAYS_LAYOUT_INL_H_
