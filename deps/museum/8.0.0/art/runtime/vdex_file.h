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

#ifndef ART_RUNTIME_VDEX_FILE_H_
#define ART_RUNTIME_VDEX_FILE_H_

#include <stdint.h>
#include <string>

#include "base/array_ref.h"
#include "base/macros.h"
#include "mem_map.h"
#include "os.h"

namespace art {

class DexFile;

// VDEX files contain extracted DEX files. The VdexFile class maps the file to
// memory and provides tools for accessing its individual sections.
//
// File format:
//   VdexFile::Header    fixed-length header
//
//   DEX[0]              array of the input DEX files
//   DEX[1]              the bytecode may have been quickened
//   ...
//   DEX[D]
//

class VdexFile {
 public:
  struct Header {
   public:
    Header(uint32_t number_of_dex_files_,
           uint32_t dex_size,
           uint32_t verifier_deps_size,
           uint32_t quickening_info_size);

    const char* GetMagic() const { return reinterpret_cast<const char*>(magic_); }
    const char* GetVersion() const { return reinterpret_cast<const char*>(version_); }
    bool IsMagicValid() const;
    bool IsVersionValid() const;
    bool IsValid() const { return IsMagicValid() && IsVersionValid(); }

    uint32_t GetDexSize() const { return dex_size_; }
    uint32_t GetVerifierDepsSize() const { return verifier_deps_size_; }
    uint32_t GetQuickeningInfoSize() const { return quickening_info_size_; }
    uint32_t GetNumberOfDexFiles() const { return number_of_dex_files_; }

    static constexpr uint8_t kVdexInvalidMagic[] = { 'w', 'd', 'e', 'x' };

   private:
    static constexpr uint8_t kVdexMagic[] = { 'v', 'd', 'e', 'x' };
    // Last update: Disable in-place vdex update
    static constexpr uint8_t kVdexVersion[] = { '0', '0', '6', '\0' };

    uint8_t magic_[4];
    uint8_t version_[4];
    uint32_t number_of_dex_files_;
    uint32_t dex_size_;
    uint32_t verifier_deps_size_;
    uint32_t quickening_info_size_;

    friend class VdexFile;
  };

  typedef uint32_t VdexChecksum;

  // Returns nullptr if the vdex file cannot be opened or is not valid.
  static std::unique_ptr<VdexFile> Open(const std::string& vdex_filename,
                                        bool writable,
                                        bool low_4gb,
                                        bool unquicken,
                                        std::string* error_msg);

  // Returns nullptr if the vdex file cannot be opened or is not valid.
  static std::unique_ptr<VdexFile> Open(int file_fd,
                                        size_t vdex_length,
                                        const std::string& vdex_filename,
                                        bool writable,
                                        bool low_4gb,
                                        bool unquicken,
                                        std::string* error_msg);

  const uint8_t* Begin() const { return mmap_->Begin(); }
  const uint8_t* End() const { return mmap_->End(); }
  size_t Size() const { return mmap_->Size(); }

  const Header& GetHeader() const {
    return *reinterpret_cast<const Header*>(Begin());
  }

  ArrayRef<const uint8_t> GetVerifierDepsData() const {
    return ArrayRef<const uint8_t>(
        DexBegin() + GetHeader().GetDexSize(), GetHeader().GetVerifierDepsSize());
  }

  ArrayRef<const uint8_t> GetQuickeningInfo() const {
    return ArrayRef<const uint8_t>(
        GetVerifierDepsData().data() + GetHeader().GetVerifierDepsSize(),
        GetHeader().GetQuickeningInfoSize());
  }

  bool IsValid() const {
    return mmap_->Size() >= sizeof(Header) && GetHeader().IsValid();
  }

  // This method is for iterating over the dex files in the vdex. If `cursor` is null,
  // the first dex file is returned. If `cursor` is not null, it must point to a dex
  // file and this method returns the next dex file if there is one, or null if there
  // is none.
  const uint8_t* GetNextDexFileData(const uint8_t* cursor) const;

  // Get the location checksum of the dex file number `dex_file_index`.
  uint32_t GetLocationChecksum(uint32_t dex_file_index) const {
    DCHECK_LT(dex_file_index, GetHeader().GetNumberOfDexFiles());
    return reinterpret_cast<const uint32_t*>(Begin() + sizeof(Header))[dex_file_index];
  }

  // Opens all the dex files contained in this vdex file.
  bool OpenAllDexFiles(std::vector<std::unique_ptr<const DexFile>>* dex_files,
                       std::string* error_msg);

  // In-place unquicken the given `dex_files` based on `quickening_info`.
  static void Unquicken(const std::vector<const DexFile*>& dex_files,
                        const ArrayRef<const uint8_t>& quickening_info);

 private:
  explicit VdexFile(MemMap* mmap) : mmap_(mmap) {}

  bool HasDexSection() const {
    return GetHeader().GetDexSize() != 0;
  }

  const uint8_t* DexBegin() const {
    return Begin() + sizeof(Header) + GetSizeOfChecksumsSection();
  }

  const uint8_t* DexEnd() const {
    return DexBegin() + GetHeader().GetDexSize();
  }

  size_t GetSizeOfChecksumsSection() const {
    return sizeof(VdexChecksum) * GetHeader().GetNumberOfDexFiles();
  }

  std::unique_ptr<MemMap> mmap_;

  DISALLOW_COPY_AND_ASSIGN(VdexFile);
};

}  // namespace art

#endif  // ART_RUNTIME_VDEX_FILE_H_
