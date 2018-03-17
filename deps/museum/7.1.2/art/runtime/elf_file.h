/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_ELF_FILE_H_
#define ART_RUNTIME_ELF_FILE_H_

#include <memory>
#include <string>

#include "base/macros.h"
// Explicitly include our own elf.h to avoid Linux and other dependencies.
#include "./elf.h"
#include "os.h"

namespace art {
template <typename ElfTypes>
class ElfFileImpl;

// Explicitly instantiated in elf_file.cc
typedef ElfFileImpl<ElfTypes32> ElfFileImpl32;
typedef ElfFileImpl<ElfTypes64> ElfFileImpl64;

// Used for compile time and runtime for ElfFile access. Because of
// the need for use at runtime, cannot directly use LLVM classes such as
// ELFObjectFile.
class ElfFile {
 public:
  static ElfFile* Open(File* file,
                       bool writable,
                       bool program_header_only,
                       bool low_4gb,
                       std::string* error_msg,
                       uint8_t* requested_base = nullptr);  // TODO: move arg to before error_msg.
  // Open with specific mmap flags, Always maps in the whole file, not just the
  // program header sections.
  static ElfFile* Open(File* file,
                       int mmap_prot,
                       int mmap_flags,
                       std::string* error_msg);
  ~ElfFile();

  // Load segments into memory based on PT_LOAD program headers
  bool Load(File* file, bool executable, bool low_4gb, std::string* error_msg);

  const uint8_t* FindDynamicSymbolAddress(const std::string& symbol_name) const;

  size_t Size() const;

  // The start of the memory map address range for this ELF file.
  uint8_t* Begin() const;

  // The end of the memory map address range for this ELF file.
  uint8_t* End() const;

  const std::string& GetFilePath() const;

  bool GetSectionOffsetAndSize(const char* section_name, uint64_t* offset, uint64_t* size) const;

  bool HasSection(const std::string& name) const;

  uint64_t FindSymbolAddress(unsigned section_type,
                             const std::string& symbol_name,
                             bool build_map);

  bool GetLoadedSize(size_t* size, std::string* error_msg) const;

  // Strip an ELF file of unneeded debugging information.
  // Returns true on success, false on failure.
  static bool Strip(File* file, std::string* error_msg);

  // Fixup an ELF file so that that oat header will be loaded at oat_begin.
  // Returns true on success, false on failure.
  static bool Fixup(File* file, uint64_t oat_data_begin);

  bool Fixup(uint64_t base_address);

  bool Is64Bit() const {
    return elf64_.get() != nullptr;
  }

  ElfFileImpl32* GetImpl32() const {
    return elf32_.get();
  }

  ElfFileImpl64* GetImpl64() const {
    return elf64_.get();
  }

 private:
  explicit ElfFile(ElfFileImpl32* elf32);
  explicit ElfFile(ElfFileImpl64* elf64);

  const std::unique_ptr<ElfFileImpl32> elf32_;
  const std::unique_ptr<ElfFileImpl64> elf64_;

  DISALLOW_COPY_AND_ASSIGN(ElfFile);
};

}  // namespace art

#endif  // ART_RUNTIME_ELF_FILE_H_
