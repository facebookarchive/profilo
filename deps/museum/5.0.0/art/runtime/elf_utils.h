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

#ifndef ART_RUNTIME_ELF_UTILS_H_
#define ART_RUNTIME_ELF_UTILS_H_

#include <sys/cdefs.h>

// Explicitly include our own elf.h to avoid Linux and other dependencies.
#include "./elf.h"

#include "base/logging.h"

// Architecture dependent flags for the ELF header.
#define EF_ARM_EABI_VER5 0x05000000
#define EF_MIPS_ABI_O32 0x00001000
#define EF_MIPS_ARCH_32R2 0x70000000

#define EI_ABIVERSION 8
#define EM_ARM 40
#define EF_MIPS_NOREORDER 1
#define EF_MIPS_PIC 2
#define EF_MIPS_CPIC 4
#define STV_DEFAULT 0

#define EM_AARCH64 183

#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH 29
#define DT_FLAGS 30

/* MIPS dependent d_tag field for Elf32_Dyn.  */
#define DT_MIPS_RLD_VERSION  0x70000001 /* Runtime Linker Interface ID */
#define DT_MIPS_TIME_STAMP   0x70000002 /* Timestamp */
#define DT_MIPS_ICHECKSUM    0x70000003 /* Cksum of ext. str. and com. sizes */
#define DT_MIPS_IVERSION     0x70000004 /* Version string (string tbl index) */
#define DT_MIPS_FLAGS        0x70000005 /* Flags */
#define DT_MIPS_BASE_ADDRESS 0x70000006 /* Segment base address */
#define DT_MIPS_CONFLICT     0x70000008 /* Adr of .conflict section */
#define DT_MIPS_LIBLIST      0x70000009 /* Address of .liblist section */
#define DT_MIPS_LOCAL_GOTNO  0x7000000a /* Number of local .GOT entries */
#define DT_MIPS_CONFLICTNO   0x7000000b /* Number of .conflict entries */
#define DT_MIPS_LIBLISTNO    0x70000010 /* Number of .liblist entries */
#define DT_MIPS_SYMTABNO     0x70000011 /* Number of .dynsym entries */
#define DT_MIPS_UNREFEXTNO   0x70000012 /* First external DYNSYM */
#define DT_MIPS_GOTSYM       0x70000013 /* First GOT entry in .dynsym */
#define DT_MIPS_HIPAGENO     0x70000014 /* Number of GOT page table entries */
#define DT_MIPS_RLD_MAP      0x70000016 /* Address of debug map pointer */

// Patching section type
#define SHT_OAT_PATCH        SHT_LOUSER

inline void SetBindingAndType(Elf32_Sym* sym, unsigned char b, unsigned char t) {
  sym->st_info = (b << 4) + (t & 0x0f);
}

inline bool IsDynamicSectionPointer(Elf32_Word d_tag, Elf32_Word e_machine) {
  switch (d_tag) {
    // case 1: well known d_tag values that imply Elf32_Dyn.d_un contains an address in d_ptr
    case DT_PLTGOT:
    case DT_HASH:
    case DT_STRTAB:
    case DT_SYMTAB:
    case DT_RELA:
    case DT_INIT:
    case DT_FINI:
    case DT_REL:
    case DT_DEBUG:
    case DT_JMPREL: {
      return true;
    }
    // d_val or ignored values
    case DT_NULL:
    case DT_NEEDED:
    case DT_PLTRELSZ:
    case DT_RELASZ:
    case DT_RELAENT:
    case DT_STRSZ:
    case DT_SYMENT:
    case DT_SONAME:
    case DT_RPATH:
    case DT_SYMBOLIC:
    case DT_RELSZ:
    case DT_RELENT:
    case DT_PLTREL:
    case DT_TEXTREL:
    case DT_BIND_NOW:
    case DT_INIT_ARRAYSZ:
    case DT_FINI_ARRAYSZ:
    case DT_RUNPATH:
    case DT_FLAGS: {
      return false;
    }
    // boundary values that should not be used
    case DT_ENCODING:
    case DT_LOOS:
    case DT_HIOS:
    case DT_LOPROC:
    case DT_HIPROC: {
      LOG(FATAL) << "Illegal d_tag value 0x" << std::hex << d_tag;
      return false;
    }
    default: {
      // case 2: "regular" DT_* ranges where even d_tag values imply an address in d_ptr
      if ((DT_ENCODING  < d_tag && d_tag < DT_LOOS)
          || (DT_LOOS   < d_tag && d_tag < DT_HIOS)
          || (DT_LOPROC < d_tag && d_tag < DT_HIPROC)) {
        // Special case for MIPS which breaks the regular rules between DT_LOPROC and DT_HIPROC
        if (e_machine == EM_MIPS) {
          switch (d_tag) {
            case DT_MIPS_RLD_VERSION:
            case DT_MIPS_TIME_STAMP:
            case DT_MIPS_ICHECKSUM:
            case DT_MIPS_IVERSION:
            case DT_MIPS_FLAGS:
            case DT_MIPS_LOCAL_GOTNO:
            case DT_MIPS_CONFLICTNO:
            case DT_MIPS_LIBLISTNO:
            case DT_MIPS_SYMTABNO:
            case DT_MIPS_UNREFEXTNO:
            case DT_MIPS_GOTSYM:
            case DT_MIPS_HIPAGENO: {
              return false;
            }
            case DT_MIPS_BASE_ADDRESS:
            case DT_MIPS_CONFLICT:
            case DT_MIPS_LIBLIST:
            case DT_MIPS_RLD_MAP: {
              return true;
            }
            default: {
              LOG(FATAL) << "Unknown MIPS d_tag value 0x" << std::hex << d_tag;
              return false;
            }
          }
        } else if ((d_tag % 2) == 0) {
          return true;
        } else {
          return false;
        }
      } else {
        LOG(FATAL) << "Unknown d_tag value 0x" << std::hex << d_tag;
        return false;
      }
    }
  }
}

#endif  // ART_RUNTIME_ELF_UTILS_H_
