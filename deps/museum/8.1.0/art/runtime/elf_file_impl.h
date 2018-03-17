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

#ifndef ART_RUNTIME_ELF_FILE_IMPL_H_
#define ART_RUNTIME_ELF_FILE_IMPL_H_

#include <map>
#include <memory>
#include <type_traits>
#include <vector>

// Explicitly include our own elf.h to avoid Linux and other dependencies.
#include "./elf.h"
#include "mem_map.h"

namespace art {

extern "C" {
  struct JITCodeEntry;
}

template <typename ElfTypes>
class ElfFileImpl {
 public:
  using Elf_Addr = typename ElfTypes::Addr;
  using Elf_Off = typename ElfTypes::Off;
  using Elf_Half = typename ElfTypes::Half;
  using Elf_Word = typename ElfTypes::Word;
  using Elf_Sword = typename ElfTypes::Sword;
  using Elf_Ehdr = typename ElfTypes::Ehdr;
  using Elf_Shdr = typename ElfTypes::Shdr;
  using Elf_Sym = typename ElfTypes::Sym;
  using Elf_Rel = typename ElfTypes::Rel;
  using Elf_Rela = typename ElfTypes::Rela;
  using Elf_Phdr = typename ElfTypes::Phdr;
  using Elf_Dyn = typename ElfTypes::Dyn;

  static ElfFileImpl* Open(File* file,
                           bool writable,
                           bool program_header_only,
                           bool low_4gb,
                           std::string* error_msg,
                           uint8_t* requested_base = nullptr);
  static ElfFileImpl* Open(File* file,
                           int mmap_prot,
                           int mmap_flags,
                           bool low_4gb,
                           std::string* error_msg);
  ~ElfFileImpl();

  const std::string& GetFilePath() const {
    return file_path_;
  }

  uint8_t* Begin() const {
    return map_->Begin();
  }

  uint8_t* End() const {
    return map_->End();
  }

  size_t Size() const {
    return map_->Size();
  }

  Elf_Ehdr& GetHeader() const;

  Elf_Word GetProgramHeaderNum() const;
  Elf_Phdr* GetProgramHeader(Elf_Word) const;

  Elf_Word GetSectionHeaderNum() const;
  Elf_Shdr* GetSectionHeader(Elf_Word) const;
  Elf_Shdr* FindSectionByType(Elf_Word type) const;
  Elf_Shdr* FindSectionByName(const std::string& name) const;

  Elf_Shdr* GetSectionNameStringSection() const;

  // Find .dynsym using .hash for more efficient lookup than FindSymbolAddress.
  const uint8_t* FindDynamicSymbolAddress(const std::string& symbol_name) const;

  static bool IsSymbolSectionType(Elf_Word section_type);
  Elf_Word GetSymbolNum(Elf_Shdr&) const;
  Elf_Sym* GetSymbol(Elf_Word section_type, Elf_Word i) const;

  // Find address of symbol in specified table, returning 0 if it is
  // not found. See FindSymbolByName for an explanation of build_map.
  Elf_Addr FindSymbolAddress(Elf_Word section_type,
                             const std::string& symbol_name,
                             bool build_map);

  // Lookup a string given string section and offset. Returns null for special 0 offset.
  const char* GetString(Elf_Shdr&, Elf_Word) const;

  Elf_Word GetDynamicNum() const;
  Elf_Dyn& GetDynamic(Elf_Word) const;

  Elf_Word GetRelNum(Elf_Shdr&) const;
  Elf_Rel& GetRel(Elf_Shdr&, Elf_Word) const;

  Elf_Word GetRelaNum(Elf_Shdr&) const;
  Elf_Rela& GetRela(Elf_Shdr&, Elf_Word) const;

  // Retrieves the expected size when the file is loaded at runtime. Returns true if successful.
  bool GetLoadedSize(size_t* size, std::string* error_msg) const;

  // Load segments into memory based on PT_LOAD program headers.
  // executable is true at run time, false at compile time.
  bool Load(File* file, bool executable, bool low_4gb, std::string* error_msg);

  bool Fixup(Elf_Addr base_address);
  bool FixupDynamic(Elf_Addr base_address);
  bool FixupSectionHeaders(Elf_Addr base_address);
  bool FixupProgramHeaders(Elf_Addr base_address);
  bool FixupSymbols(Elf_Addr base_address, bool dynamic);
  bool FixupRelocations(Elf_Addr base_address);
  bool FixupDebugSections(Elf_Addr base_address_delta);
  bool ApplyOatPatchesTo(const char* target_section_name, Elf_Addr base_address_delta);
  static void ApplyOatPatches(const uint8_t* patches, const uint8_t* patches_end, Elf_Addr delta,
                              uint8_t* to_patch, const uint8_t* to_patch_end);

  bool Strip(File* file, std::string* error_msg);

 private:
  ElfFileImpl(File* file, bool writable, bool program_header_only, uint8_t* requested_base);

  bool Setup(File* file, int prot, int flags, bool low_4gb, std::string* error_msg);

  bool SetMap(File* file, MemMap* map, std::string* error_msg);

  uint8_t* GetProgramHeadersStart() const;
  uint8_t* GetSectionHeadersStart() const;
  Elf_Phdr& GetDynamicProgramHeader() const;
  Elf_Dyn* GetDynamicSectionStart() const;
  Elf_Sym* GetSymbolSectionStart(Elf_Word section_type) const;
  const char* GetStringSectionStart(Elf_Word section_type) const;
  Elf_Rel* GetRelSectionStart(Elf_Shdr&) const;
  Elf_Rela* GetRelaSectionStart(Elf_Shdr&) const;
  Elf_Word* GetHashSectionStart() const;
  Elf_Word GetHashBucketNum() const;
  Elf_Word GetHashChainNum() const;
  Elf_Word GetHashBucket(size_t i, bool* ok) const;
  Elf_Word GetHashChain(size_t i, bool* ok) const;

  typedef std::map<std::string, Elf_Sym*> SymbolTable;
  SymbolTable** GetSymbolTable(Elf_Word section_type);

  bool ValidPointer(const uint8_t* start) const;

  const Elf_Sym* FindDynamicSymbol(const std::string& symbol_name) const;

  // Check that certain sections and their dependencies exist.
  bool CheckSectionsExist(File* file, std::string* error_msg) const;

  // Check that the link of the first section links to the second section.
  bool CheckSectionsLinked(const uint8_t* source, const uint8_t* target) const;

  // Check whether the offset is in range, and set to target to Begin() + offset if OK.
  bool CheckAndSet(Elf32_Off offset, const char* label, uint8_t** target, std::string* error_msg);

  // Find symbol in specified table, returning null if it is not found.
  //
  // If build_map is true, builds a map to speed repeated access. The
  // map does not included untyped symbol values (aka STT_NOTYPE)
  // since they can contain duplicates. If build_map is false, the map
  // will be used if it was already created. Typically build_map
  // should be set unless only a small number of symbols will be
  // looked up.
  Elf_Sym* FindSymbolByName(Elf_Word section_type,
                            const std::string& symbol_name,
                            bool build_map);

  Elf_Phdr* FindProgamHeaderByType(Elf_Word type) const;

  Elf_Dyn* FindDynamicByType(Elf_Sword type) const;
  Elf_Word FindDynamicValueByType(Elf_Sword type) const;

  // Lookup a string by section type. Returns null for special 0 offset.
  const char* GetString(Elf_Word section_type, Elf_Word) const;

  const std::string file_path_;
  const bool writable_;
  const bool program_header_only_;

  // ELF header mapping. If program_header_only_ is false, will
  // actually point to the entire elf file.
  std::unique_ptr<MemMap> map_;
  Elf_Ehdr* header_;
  std::vector<MemMap*> segments_;

  // Pointer to start of first PT_LOAD program segment after Load()
  // when program_header_only_ is true.
  uint8_t* base_address_;

  // The program header should always available but use GetProgramHeadersStart() to be sure.
  uint8_t* program_headers_start_;

  // Conditionally available values. Use accessors to ensure they exist if they are required.
  uint8_t* section_headers_start_;
  Elf_Phdr* dynamic_program_header_;
  Elf_Dyn* dynamic_section_start_;
  Elf_Sym* symtab_section_start_;
  Elf_Sym* dynsym_section_start_;
  char* strtab_section_start_;
  char* dynstr_section_start_;
  Elf_Word* hash_section_start_;

  SymbolTable* symtab_symbol_table_;
  SymbolTable* dynsym_symbol_table_;

  // Override the 'base' p_vaddr in the first LOAD segment with this value (if non-null).
  uint8_t* requested_base_;

  DISALLOW_COPY_AND_ASSIGN(ElfFileImpl);
};

}  // namespace art

#endif  // ART_RUNTIME_ELF_FILE_IMPL_H_
