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

#include "bionic_linker.h"

#include <string.h>

/*
 * Simple (very) elf parser for finding plt relocations
 *
 * tl;dr
 *
 * Basically it parses the header and go through program sections to find
 * out first PT_LOAD section (needed for figuring out load bias) and PT_DYNAMIC
 * section.
 * Then, parses dynamic section looking for PLT relocation table and dynamic
 * symbols table and dynamic strings table
 */
static const uint8_t EXPECTED_ELF_HEADER_IDENT[] =
    {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS32, ELFDATA2LSB, EV_CURRENT};

class ElfPLTRelParser {
  public:
    ElfPLTRelParser(const void* map_address)
        : mLoadBias(nullptr),
          mPltRelTable(nullptr),
          mPltRelTableSize(0),
          mDynSymbolsTable(nullptr),
          mDynStrsTable(nullptr) {
      if (!map_address) {
        return;
      }
      const Elf32_Ehdr* header = ElfPLTRelParser::checkHeader(map_address);
      if (!header) {
        return;
      }

      const Elf32_Phdr* phdr =
          reinterpret_cast<const Elf32_Phdr*>((const uint8_t*)map_address + header->e_phoff);
      const Elf32_Dyn* dynamic_table = nullptr;
      if (!ElfPLTRelParser::parseSegments(
            map_address, phdr, header->e_phnum, mLoadBias, dynamic_table)) {
        return;
      }
      parseDynamicSegment(dynamic_table);
    }

    void* getPltGotEntry(const char* name) {
      if (!mPltRelTable || !mPltRelTableSize || !mDynSymbolsTable || !mDynStrsTable) {
        return nullptr;
      }

      for (unsigned int i = 0; i < mPltRelTableSize; ++i) {
        Elf32_Rel rel = mPltRelTable[i];
        int rel_type = ELF32_R_TYPE(rel.r_info);
        int rel_sym = ELF32_R_SYM(rel.r_info);
        if (rel_type != PLT_RELOCATION_TYPE || !rel_sym) {
          continue;
        }

        Elf32_Sym symbol = mDynSymbolsTable[rel_sym];
        const char* symbol_name = mDynStrsTable + symbol.st_name;
        if (strcmp(symbol_name, name) == 0) {
          return (void*)(mLoadBias + rel.r_offset);
        }
      }

      return nullptr;
    }

  private:
    static const Elf32_Ehdr* checkHeader(const void* map_address) {
      const Elf32_Ehdr* header = reinterpret_cast<const Elf32_Ehdr*>(map_address);
      if (memcmp(EXPECTED_ELF_HEADER_IDENT,
            header->e_ident, sizeof(EXPECTED_ELF_HEADER_IDENT)) != 0) {
        return nullptr;
      }
      return header;
    }

    static bool parseSegments(
        const void* map_address,
        const Elf32_Phdr* phdr,
        Elf32_Half phnum,
        const uint8_t*& load_bias,
        const Elf32_Dyn*& dynamic_table) {
      bool load_bias_found = false;
      Elf32_Addr dynamic_vaddr = 0;
      bool dynamic_found = false;

      for (int i = 0; i < phnum; ++i) {
        if (phdr->p_type == PT_LOAD && !load_bias_found) {
          load_bias = (const uint8_t*)map_address - phdr->p_vaddr;
          load_bias_found = true;
        } else if (phdr->p_type == PT_DYNAMIC) {
          dynamic_vaddr = phdr->p_vaddr;
          dynamic_found = true;
        }
        ++phdr;
      }

      if (!load_bias || !dynamic_found) {
        return false;
      }

      dynamic_table = reinterpret_cast<const Elf32_Dyn*>(load_bias + dynamic_vaddr);
      return true;
    }

    void parseDynamicSegment(const Elf32_Dyn* dynamic_table) {
      for (const Elf32_Dyn* entry = dynamic_table; entry->d_tag != DT_NULL; ++entry) {
        switch (entry->d_tag) {
          case DT_PLTRELSZ:
            mPltRelTableSize = entry->d_un.d_val / sizeof(Elf32_Rel);
            break;
          case DT_JMPREL:
            mPltRelTable =
                reinterpret_cast<const Elf32_Rel*>(mLoadBias + entry->d_un.d_ptr);
            break;
          case DT_SYMTAB:
            mDynSymbolsTable =
                reinterpret_cast<const Elf32_Sym*>(mLoadBias + entry->d_un.d_ptr);
            break;
          case DT_STRTAB:
            mDynStrsTable =
                reinterpret_cast<const char *>(mLoadBias + entry->d_un.d_ptr);
            break;
        }
      }
    }

  private:
    const uint8_t* mLoadBias;
    const Elf32_Rel* mPltRelTable;
    size_t mPltRelTableSize;
    const Elf32_Sym* mDynSymbolsTable;
    const char* mDynStrsTable;
};
