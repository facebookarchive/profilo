/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_OAT_FILE_INL_H_
#define ART_RUNTIME_OAT_FILE_INL_H_

#include "oat_file.h"
#include "oat_quick_method_header.h"

namespace art {

inline const OatQuickMethodHeader* OatFile::OatMethod::GetOatQuickMethodHeader() const {
  const void* code = EntryPointToCodePointer(GetOatPointer<const void*>(code_offset_));
  if (code == nullptr) {
    return nullptr;
  }
  // Return a pointer to the packed struct before the code.
  return reinterpret_cast<const OatQuickMethodHeader*>(code) - 1;
}

inline uint32_t OatFile::OatMethod::GetOatQuickMethodHeaderOffset() const {
  const OatQuickMethodHeader* method_header = GetOatQuickMethodHeader();
  if (method_header == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(method_header) - begin_;
}

inline uint32_t OatFile::OatMethod::GetQuickCodeSizeOffset() const {
  const OatQuickMethodHeader* method_header = GetOatQuickMethodHeader();
  if (method_header == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(&method_header->code_size_) - begin_;
}

inline size_t OatFile::OatMethod::GetFrameSizeInBytes() const {
  const void* code = EntryPointToCodePointer(GetQuickCode());
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].frame_info_.FrameSizeInBytes();
}

inline uint32_t OatFile::OatMethod::GetCoreSpillMask() const {
  const void* code = EntryPointToCodePointer(GetQuickCode());
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].frame_info_.CoreSpillMask();
}

inline uint32_t OatFile::OatMethod::GetFpSpillMask() const {
  const void* code = EntryPointToCodePointer(GetQuickCode());
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].frame_info_.FpSpillMask();
}

inline uint32_t OatFile::OatMethod::GetVmapTableOffset() const {
  const uint8_t* vmap_table = GetVmapTable();
  return static_cast<uint32_t>(vmap_table != nullptr ? vmap_table - begin_ : 0u);
}

inline uint32_t OatFile::OatMethod::GetVmapTableOffsetOffset() const {
  const OatQuickMethodHeader* method_header = GetOatQuickMethodHeader();
  if (method_header == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(&method_header->vmap_table_offset_) - begin_;
}

inline const uint8_t* OatFile::OatMethod::GetVmapTable() const {
  const void* code = EntryPointToCodePointer(GetOatPointer<const void*>(code_offset_));
  if (code == nullptr) {
    return nullptr;
  }
  uint32_t offset = reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].vmap_table_offset_;
  if (UNLIKELY(offset == 0u)) {
    return nullptr;
  }
  return reinterpret_cast<const uint8_t*>(code) - offset;
}

inline uint32_t OatFile::OatMethod::GetQuickCodeSize() const {
  const void* code = EntryPointToCodePointer(GetOatPointer<const void*>(code_offset_));
  if (code == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const OatQuickMethodHeader*>(code)[-1].code_size_;
}

inline uint32_t OatFile::OatMethod::GetCodeOffset() const {
  return (GetQuickCodeSize() == 0) ? 0 : code_offset_;
}

inline const void* OatFile::OatMethod::GetQuickCode() const {
  return GetOatPointer<const void*>(GetCodeOffset());
}

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_INL_H_
