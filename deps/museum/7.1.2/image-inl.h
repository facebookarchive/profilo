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

#ifndef ART_RUNTIME_IMAGE_INL_H_
#define ART_RUNTIME_IMAGE_INL_H_

#include "image.h"

#include "art_method.h"
#include "imtable.h"

namespace art {

template <ReadBarrierOption kReadBarrierOption>
inline mirror::Object* ImageHeader::GetImageRoot(ImageRoot image_root) const {
  mirror::ObjectArray<mirror::Object>* image_roots = GetImageRoots<kReadBarrierOption>();
  return image_roots->Get<kVerifyNone, kReadBarrierOption>(static_cast<int32_t>(image_root));
}

template <ReadBarrierOption kReadBarrierOption>
inline mirror::ObjectArray<mirror::Object>* ImageHeader::GetImageRoots() const {
  // Need a read barrier as it's not visited during root scan.
  // Pass in the address of the local variable to the read barrier
  // rather than image_roots_ because it won't move (asserted below)
  // and it's a const member.
  mirror::ObjectArray<mirror::Object>* image_roots =
      reinterpret_cast<mirror::ObjectArray<mirror::Object>*>(image_roots_);
  mirror::ObjectArray<mirror::Object>* result =
      ReadBarrier::BarrierForRoot<mirror::ObjectArray<mirror::Object>, kReadBarrierOption>(
          &image_roots);
  DCHECK_EQ(image_roots, result);
  return image_roots;
}

template <typename Visitor>
inline void ImageHeader::VisitPackedImTables(const Visitor& visitor,
                                             uint8_t* base,
                                             size_t pointer_size) const {
  const ImageSection& section = GetImageSection(kSectionImTables);
  for (size_t pos = 0; pos < section.Size();) {
    ImTable* imt = reinterpret_cast<ImTable*>(base + section.Offset() + pos);
    for (size_t i = 0; i < ImTable::kSize; ++i) {
      ArtMethod* orig = imt->Get(i, pointer_size);
      ArtMethod* updated = visitor(orig);
      if (updated != orig) {
        imt->Set(i, updated, pointer_size);
      }
    }
    pos += ImTable::SizeInBytes(pointer_size);
  }
}

template <typename Visitor>
inline void ImageHeader::VisitPackedImtConflictTables(const Visitor& visitor,
                                                      uint8_t* base,
                                                      size_t pointer_size) const {
  const ImageSection& section = GetImageSection(kSectionIMTConflictTables);
  for (size_t pos = 0; pos < section.Size(); ) {
    auto* table = reinterpret_cast<ImtConflictTable*>(base + section.Offset() + pos);
    table->Visit([&visitor](const std::pair<ArtMethod*, ArtMethod*>& methods) {
      return std::make_pair(visitor(methods.first), visitor(methods.second));
    }, pointer_size);
    pos += table->ComputeSize(pointer_size);
  }
}

}  // namespace art

#endif  // ART_RUNTIME_IMAGE_INL_H_
