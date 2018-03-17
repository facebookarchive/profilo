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

#ifndef ART_RUNTIME_IMAGE_H_
#define ART_RUNTIME_IMAGE_H_

#include <string.h>

#include "globals.h"
#include "mirror/object.h"

namespace art {

class PACKED(4) ImageSection {
 public:
  ImageSection() : offset_(0), size_(0) { }
  ImageSection(uint32_t offset, uint32_t size) : offset_(offset), size_(size) { }
  ImageSection(const ImageSection& section) = default;
  ImageSection& operator=(const ImageSection& section) = default;

  uint32_t Offset() const {
    return offset_;
  }

  uint32_t Size() const {
    return size_;
  }

  uint32_t End() const {
    return Offset() + Size();
  }

  bool Contains(uint64_t offset) const {
    return offset - offset_ < size_;
  }

 private:
  uint32_t offset_;
  uint32_t size_;
};

// header of image files written by ImageWriter, read and validated by Space.
class PACKED(4) ImageHeader {
 public:
  ImageHeader() : compile_pic_(0) {}

  ImageHeader(uint32_t image_begin,
              uint32_t image_size_,
              ImageSection* sections,
              uint32_t image_roots,
              uint32_t oat_checksum,
              uint32_t oat_file_begin,
              uint32_t oat_data_begin,
              uint32_t oat_data_end,
              uint32_t oat_file_end,
              uint32_t pointer_size,
              bool compile_pic_);

  bool IsValid() const;
  const char* GetMagic() const;

  uint8_t* GetImageBegin() const {
    return reinterpret_cast<uint8_t*>(image_begin_);
  }

  size_t GetImageSize() const {
    return static_cast<uint32_t>(image_size_);
  }

  uint32_t GetOatChecksum() const {
    return oat_checksum_;
  }

  void SetOatChecksum(uint32_t oat_checksum) {
    oat_checksum_ = oat_checksum;
  }

  uint8_t* GetOatFileBegin() const {
    return reinterpret_cast<uint8_t*>(oat_file_begin_);
  }

  uint8_t* GetOatDataBegin() const {
    return reinterpret_cast<uint8_t*>(oat_data_begin_);
  }

  uint8_t* GetOatDataEnd() const {
    return reinterpret_cast<uint8_t*>(oat_data_end_);
  }

  uint8_t* GetOatFileEnd() const {
    return reinterpret_cast<uint8_t*>(oat_file_end_);
  }

  uint32_t GetPointerSize() const {
    return pointer_size_;
  }

  off_t GetPatchDelta() const {
    return patch_delta_;
  }

  static std::string GetOatLocationFromImageLocation(const std::string& image) {
    std::string oat_filename = image;
    if (oat_filename.length() <= 3) {
      oat_filename += ".oat";
    } else {
      oat_filename.replace(oat_filename.length() - 3, 3, "oat");
    }
    return oat_filename;
  }

  enum ImageMethod {
    kResolutionMethod,
    kImtConflictMethod,
    kImtUnimplementedMethod,
    kCalleeSaveMethod,
    kRefsOnlySaveMethod,
    kRefsAndArgsSaveMethod,
    kImageMethodsCount,  // Number of elements in enum.
  };

  enum ImageRoot {
    kDexCaches,
    kClassRoots,
    kImageRootsMax,
  };

  enum ImageSections {
    kSectionObjects,
    kSectionArtFields,
    kSectionArtMethods,
    kSectionInternedStrings,
    kSectionImageBitmap,
    kSectionCount,  // Number of elements in enum.
  };

  ArtMethod* GetImageMethod(ImageMethod index) const;
  void SetImageMethod(ImageMethod index, ArtMethod* method);

  const ImageSection& GetImageSection(ImageSections index) const;
  const ImageSection& GetMethodsSection() const {
    return GetImageSection(kSectionArtMethods);
  }

  mirror::Object* GetImageRoot(ImageRoot image_root) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::ObjectArray<mirror::Object>* GetImageRoots() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void RelocateImage(off_t delta);

  bool CompilePic() const {
    return compile_pic_ != 0;
  }

 private:
  static const uint8_t kImageMagic[4];
  static const uint8_t kImageVersion[4];

  uint8_t magic_[4];
  uint8_t version_[4];

  // Required base address for mapping the image.
  uint32_t image_begin_;

  // Image size, not page aligned.
  uint32_t image_size_;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum_;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin_;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin_;

  // End of oat data address range for this image file.
  uint32_t oat_data_end_;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end_;

  // The total delta that this image has been patched.
  int32_t patch_delta_;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots_;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size_;

  // Boolean (0 or 1) to denote if the image was compiled with --compile-pic option
  const uint32_t compile_pic_;

  // Image sections
  ImageSection sections_[kSectionCount];

  // Image methods.
  uint64_t image_methods_[kImageMethodsCount];

  friend class ImageWriter;
};

std::ostream& operator<<(std::ostream& os, const ImageHeader::ImageMethod& policy);
std::ostream& operator<<(std::ostream& os, const ImageHeader::ImageRoot& policy);
std::ostream& operator<<(std::ostream& os, const ImageHeader::ImageSections& section);
std::ostream& operator<<(std::ostream& os, const ImageSection& section);

}  // namespace art

#endif  // ART_RUNTIME_IMAGE_H_
