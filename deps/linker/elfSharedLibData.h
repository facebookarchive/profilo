/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <linker/bionic_linker.h>

#include <vector>
#include <stdexcept>
#include <string>

namespace facebook {
namespace linker {

// Android uses RELA for aarch64 and x86_64. mips64 still uses REL.
#if defined(__aarch64__) || defined(__x86_64__)
typedef ElfW(Rela) Elf_Reloc;
#else
typedef ElfW(Rel) Elf_Reloc;
#endif

class elfSharedLibData {
public:

  elfSharedLibData();
  explicit elfSharedLibData(ElfW(Addr) addr, const char* name, const ElfW(Phdr)* phdr, ElfW(Half) phnum);

  /**
   * Returns a pointer to the ElfW(Sym) for the given symbol name.
   */
  ElfW(Sym) const* find_symbol_by_name(char const* name);

  /**
   * Returns a vector of all fixed-up memory addresses that point to symbol
   */
  std::vector<void**> get_relocations(void* symbol);

  /**
   * Returns a vector of all fixed-up PLT entries that point to the symbol represented
   * by elf_sym
   */
  std::vector<void**> get_plt_relocations(ElfW(Sym) const* elf_sym);

  /**
   * Returns a vector of all fixed-up PLT entries that point to the target address
   */
  std::vector<void**> get_plt_relocations(void const* addr);

  /*
   * Returns the loaded address of the shared library
   */
  uintptr_t getLoadBias() const {
    return data_.loadBias;
  }

  /**
   * Finds the actual in-memory address of the given symbol
   */
  void* getLoadedAddress(ElfW(Sym) const* sym) const {
    return reinterpret_cast<void*>(data_.loadBias + sym->st_value);
  };

  /**
   * Returns whether or not we will use the GNU hash table instead of the ELF hash table
   */
  bool usesGnuHashTable();

  /**
   * Checks validity of this structure, including whether or not its tables are still
   * valid in our virtual memory. Not const as it can trigger lazy parsing.
   */
  bool valid();

  bool operator==(elfSharedLibData const& other) const {
    return data_.loadBias == other.data_.loadBias;
  }

  bool operator!=(elfSharedLibData const& other) const {
    return !(*this == other);
  }

  const char* getLibName() const {
    return data_.name.c_str();
  }

public:

  ElfW(Sym) const* elf_find_symbol_by_name(char const*) const;
  ElfW(Sym) const* gnu_find_symbol_by_name(char const*) const;
  std::vector<void**> get_relocations_internal(void*, Elf_Reloc const*, size_t) const;
  bool is_complete() const;
  bool parse_input();

  struct {
    uintptr_t loadBias;
    std::string name;
    ElfW(Phdr) const* phdrs;
    ElfW(Half) phnum;
  } data_;

  struct {
    bool successful;
    bool attempted;

    Elf_Reloc const *pltRelocations;
    size_t pltRelocationsLen;
    Elf_Reloc const *relocations;
    size_t relocationsLen;
    ElfW(Sym) const *dynSymbolsTable;
    char const *dynStrsTable;

    struct {
      uint32_t numbuckets;
      uint32_t numchains;
      uint32_t const *buckets;
      uint32_t const *chains;
    } elfHash;

    struct {
      uint32_t numbuckets;
      uint32_t symoffset;
      uint32_t bloom_size;
      uint32_t bloom_shift;
      ElfW(Addr) const *bloom_filter;
      uint32_t const *buckets;
      uint32_t const *chains;
    } gnuHash;
  } parsed_state_;

  static constexpr uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
};

} } // namespace facebook::linker
