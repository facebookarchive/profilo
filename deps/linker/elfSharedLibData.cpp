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

#include <linker/bionic_linker.h>
#include <linker/locks.h>
#include <linker/elfSharedLibData.h>

#include <build/build.h>

#include <sys/system_properties.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdexcept>
#include <string.h>

#define R_386_JUMP_SLOT                 7
#define R_ARM_JUMP_SLOT                 22
#define R_X86_64_JUMP_SLOT	            7
#define R_AARCH64_JUMP_SLOT             1026

#if defined(__arm__)
#define PLT_RELOCATION_TYPE R_ARM_JUMP_SLOT
#elif defined(__i386__)
#define PLT_RELOCATION_TYPE R_386_JUMP_SLOT
#elif defined(__aarch64__)
#define PLT_RELOCATION_TYPE R_AARCH64_JUMP_SLOT
#elif defined(__x86_64__)
#define PLT_RELOCATION_TYPE R_X86_64_JUMP_SLOT
#else
#error invalid arch
#endif

#define DT_GNU_HASH	0x6ffffef5	/* GNU-style hash table.  */

#if defined(__x86_64__) || defined(__aarch64__)
# define ELF_RELOC_TYPE ELF64_R_TYPE
# define ELF_RELOC_SYM  ELF64_R_SYM
#else
# define ELF_RELOC_TYPE ELF32_R_TYPE
# define ELF_RELOC_SYM  ELF32_R_SYM
#endif

namespace facebook {
namespace linker {

namespace {

static uint32_t elfhash(const char* name) {
  uint32_t h = 0, g;

  while (*name) {
    h = (h << 4) + *name++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

static uint32_t gnuhash(char const* name) {
  uint32_t h = 5381;
  while (*name != 0) {
    h += (h << 5) + *name++; // h*33 + c == h + h * 32 + c == h + h << 5 + c
  }

  return h;
}

} // namespace (anonymous)

// pltRelocations is explicitly set to nullptr as a sentinel to operator bool
elfSharedLibData::elfSharedLibData() {}

elfSharedLibData::elfSharedLibData(dl_phdr_info const* info) {
  ElfW(Dyn) const* dynamic_table = nullptr;

  loadBias = info->dlpi_addr;
  libName = info->dlpi_name;

  for (int i = 0; i < info->dlpi_phnum; ++i) {
    ElfW(Phdr) const* phdr = &info->dlpi_phdr[i];
    if (phdr->p_type == PT_DYNAMIC) {
      dynamic_table = reinterpret_cast<ElfW(Dyn) const*>(loadBias + phdr->p_vaddr);
      break;
    }
  }

  if (!dynamic_table) {
    throw input_parse_error("dynamic_table == null");
  }

  for (ElfW(Dyn) const* entry = dynamic_table; entry && entry->d_tag != DT_NULL; ++entry) {
    switch (entry->d_tag) {
      case DT_PLTRELSZ:
        pltRelocationsLen = entry->d_un.d_val / sizeof(Elf_Reloc);
        break;

      case DT_JMPREL: // DT_PLTREL just declares the Rel/Rela type in use, not the table data
        pltRelocations =
          reinterpret_cast<Elf_Reloc const*>(loadBias + entry->d_un.d_ptr);
        break;

      case DT_RELSZ:
      case DT_RELASZ:
        // bionic's linker already handles sanity checking/blowing up if wrong Rel/Rela match
        relocationsLen = entry->d_un.d_val / sizeof(Elf_Reloc);
        break;

      case DT_REL:
      case DT_RELA:
        // bionic's linker already handles sanity checking/blowing up if wrong Rel/Rela match
        relocations =
          reinterpret_cast<Elf_Reloc const*>(loadBias + entry->d_un.d_ptr);
        break;

      // TODO (t30088113): handle DT_ANDROID_REL[A][SZ]

      case DT_SYMTAB:
        dynSymbolsTable =
          reinterpret_cast<ElfW(Sym) const*>(loadBias + entry->d_un.d_ptr);
        break;

      case DT_STRTAB:
        dynStrsTable =
          reinterpret_cast<char const*>(loadBias + entry->d_un.d_ptr);
        break;

      case DT_HASH:
        elfHash_.numbuckets_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr)[0];
        elfHash_.numchains_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr)[1];
        elfHash_.buckets_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr + 8);
        // chains_ is stored immediately after buckets_ and is the same type, so the index after
        // the last valid bucket value is the first valid chain value.
        elfHash_.chains_ = &elfHash_.buckets_[elfHash_.numbuckets_];
        break;

      case DT_GNU_HASH: // see http://www.linker-aliens.org/blogs/ali/entry/gnu_hash_elf_sections/
        // the original AOSP code uses several binary-math optimizations that differ from the "standard"
        // gnu hash implementation, and have been left in place with explanatory comments to avoid diverging
        gnuHash_.numbuckets_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr)[0];
        gnuHash_.symoffset_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr)[1];
        gnuHash_.bloom_size_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr)[2];
        gnuHash_.bloom_shift_ = reinterpret_cast<uint32_t const*>(loadBias + entry->d_un.d_ptr)[3];
        gnuHash_.bloom_filter_ = reinterpret_cast<ElfW(Addr) const*>(loadBias + entry->d_un.d_ptr + 16);
        gnuHash_.buckets_ = reinterpret_cast<uint32_t const*>(&gnuHash_.bloom_filter_[gnuHash_.bloom_size_]);

        // chains_ is stored immediately after buckets_ and is the same type, so the index after
        // the last valid bucket value is the first valid chain value.
        // However, note that we subtract symoffset_ (and thus actually start the chains_ array
        // INSIDE the buckets_ array)! This is because the chains_ index for a symbol is negatively
        // offset from its dynSymbolsTable index by symoffset_. Normally, once you find a match in
        // chains_ you'd add symoffset_ and then you'd have your dynSymbolsTable index... but by
        // "shifting" the array backwards we can make the chains_ indices line up exactly with
        // dynSymbolsTable right from the start.
        // We don't have to ever worry about indexing into invalid chains_ data, because the
        // chain-start indices that live in buckets_ are indices into dynSymbolsTable and will thus
        // also never be less than symoffset_.
        gnuHash_.chains_ = gnuHash_.buckets_ + gnuHash_.numbuckets_ - gnuHash_.symoffset_;

        // verify that bloom_size_ is a power of 2
        if ((((uint32_t)(gnuHash_.bloom_size_ - 1)) & gnuHash_.bloom_size_) != 0) {
          // shouldn't be possible; the android linker has already performed this check
          throw input_parse_error("bloom_size_ not power of 2");
        }
        // since we know that bloom_size_ is a power of two, we can simplify modulus division later in
        // gnu_find_symbol_by_name by decrementing by 1 here and then using logical-AND instead of mod-div
        // in the lookup (0x100 - 1 == 0x0ff, 0x1c3 & 0x0ff == 0x0c3.. the "remainder")
        gnuHash_.bloom_size_--;
        break;
    }

    if (is_complete()) {
      break;
    }
  }

  if (!is_complete()) {
    // Error, go to next library
    throw input_parse_error("not all info found");
  }
}

#ifndef __LP64__
elfSharedLibData::elfSharedLibData(soinfo const* si) {
  pltRelocationsLen = si->plt_rel_count;
  pltRelocations = si->plt_rel;
  relocationsLen = si->rel_count;
  relocations = si->rel;
  dynSymbolsTable = si->symtab;
  dynStrsTable = si->strtab;

  elfHash_.numbuckets_ = si->nbucket;
  elfHash_.numchains_ = si->nchain;
  elfHash_.buckets_ = si->bucket;
  elfHash_.chains_ = si->chain;

  gnuHash_ = {};

  if (facebook::build::getAndroidSdk() >= 17) {
    loadBias = si->load_bias;
  } else {
    loadBias = si->base;
  }

  libName = si->name;
}
#endif

ElfW(Sym) const* elfSharedLibData::find_symbol_by_name(char const* name) const {
  auto sym = usesGnuHashTable() ? gnu_find_symbol_by_name(name)
                                : elf_find_symbol_by_name(name);

  // the GNU hash table doesn't include entries for any STN_UNDEF symbols in the object,
  // and although the ELF hash table "should" according to the spec contain entries for
  // every symbol, there may be some noncompliant binaries out in the world.

  // I *think* that if a symbol is STN_UNDEF, it's "extern"-linked and will thus have an
  // entry in either the DT_JMPREL[A] or DT_REL[A] sections. Not entirely sure though.

  // note the !sym check in each loop: don't perform this work if we've already found it.
  for (size_t i = 0; !sym && i < pltRelocationsLen; i++) {
    size_t sym_idx = ELF_RELOC_SYM(pltRelocations[i].r_info);
    if (strcmp(dynStrsTable + dynSymbolsTable[sym_idx].st_name, name) == 0) {
      sym = &dynSymbolsTable[sym_idx];
    }
  }
  for (size_t i = 0; !sym && i < relocationsLen; i++) {
    size_t sym_idx = ELF_RELOC_SYM(relocations[i].r_info);
    if (strcmp(dynStrsTable + dynSymbolsTable[sym_idx].st_name, name) == 0) {
      sym = &dynSymbolsTable[sym_idx];
    }
  }

  return sym;
}

ElfW(Sym) const* elfSharedLibData::elf_find_symbol_by_name(char const* name) const {
  uint32_t hash = elfhash(name);

  for (uint32_t n = elfHash_.buckets_[hash % elfHash_.numbuckets_]; n != 0; n = elfHash_.chains_[n]) {
    ElfW(Sym) const* s = dynSymbolsTable + n; // identical to &dynSymbolsTable[n]
    if (strcmp(dynStrsTable + s->st_name, name) == 0) {
      return s;
    }
  }

  return nullptr;
}

// the original AOSP code uses several binary-math optimizations that differ from the "standard"
// gnu hash implementation, and have been left in place with explanatory comments to avoid diverging
ElfW(Sym) const* elfSharedLibData::gnu_find_symbol_by_name(char const* name) const {
  uint32_t hash = gnuhash(name);
  uint32_t h2 = hash >> gnuHash_.bloom_shift_;

  // bloom_size_ has been decremented by 1 from its original value (which was guaranteed
  // to be a power of two), meaning that this is mathematically equivalent to modulus division:
  // 0x100 - 1 == 0x0ff, and 0x1c3 & 0x0ff = 0x0c3.. the "remainder"
  uint32_t word_num = (hash / kBloomMaskBits) & gnuHash_.bloom_size_;
  ElfW(Addr) bloom_word = gnuHash_.bloom_filter_[word_num];

  // test against bloom filter
  if ((1 & (bloom_word >> (hash % kBloomMaskBits)) & (bloom_word >> (h2 % kBloomMaskBits))) == 0) {
    return nullptr;
  }

  // bloom test says "probably yes"...
  uint32_t n = gnuHash_.buckets_[hash % gnuHash_.numbuckets_];

  if (n == 0) {
    return nullptr;
  }

  do {
    ElfW(Sym) const* s = dynSymbolsTable + n; // identical to &dynSymbolsTable[n]
    // this XOR is mathematically equivalent to (hash1 | 1) == (hash2 | 1), but faster
    // basically, test for equality while ignoring LSB
    if (((gnuHash_.chains_[n] ^ hash) >> 1) == 0 &&
        strcmp(dynStrsTable + s->st_name, name) == 0) {
      return s;
    }
  } while ((gnuHash_.chains_[n++] & 1) == 0); // gnu hash chains use the LSB as an end-of-chain marker

  return nullptr;
}

std::vector<void**> elfSharedLibData::get_relocations(void *symbol) const {
  std::vector<void**> relocs;

  for (size_t i = 0; i < relocationsLen; i++) {
    auto const& relocation = relocations[i];
    void** relocated = reinterpret_cast<void**>(loadBias + relocation.r_offset);
    if (*relocated == symbol) {
      relocs.push_back(relocated);
    }
  }

  return relocs;
}

std::vector<void**> elfSharedLibData::get_plt_relocations(ElfW(Sym) const* elf_sym) const {
  // TODO: merge this and get_relocations into one helper-based implementation
  std::vector<void**> relocs;

  for (unsigned int i = 0; i < pltRelocationsLen; i++) {
    Elf_Reloc const& rel = pltRelocations[i];

    // is this necessary? will there ever be a type of relocation in pltRelocations
    // that points to symbol and we *don't* want to fix up?
    if (ELF_RELOC_TYPE(rel.r_info) != PLT_RELOCATION_TYPE) {
      continue;
    }

    if (&dynSymbolsTable[ELF_RELOC_SYM(rel.r_info)] == elf_sym) {
      // Found the address of the relocation
      void** plt_got_entry = reinterpret_cast<void**>(loadBias + rel.r_offset);
      relocs.push_back(plt_got_entry);
    }
  }

  return relocs;
}

bool elfSharedLibData::is_complete() const {
  return pltRelocationsLen && pltRelocations &&
         // relocationsLen && relocations &&     TODO (t30088113): re-enable when DT_ANDROID_REL is supported
         dynSymbolsTable && dynStrsTable &&
         (elfHash_.numbuckets_ > 0 || gnuHash_.numbuckets_ > 0);
}

/* It can happen that, after caching a shared object's data in sharedLibData,
 * the library is unloaded, so references to memory in that address space
 * result in SIGSEGVs. Thus, check here that the addresses are still valid.
*/
elfSharedLibData::operator bool() const {
  Dl_info info;

  if (!is_complete()) {
    return false;
  }

  // pltRelocations is somewhat special: the "bad" constructor explicitly sets
  // this to nullptr in order to mark the entire object as invalid. if this check
  // is removed, be sure to use some other form of sentinel.
  if (!pltRelocations ||
      !dladdr(pltRelocations, &info) ||
      strcmp(info.dli_fname, libName) != 0) {
    return false;
  }

  if (!dynSymbolsTable ||
      !dladdr(dynSymbolsTable, &info) ||
      strcmp(info.dli_fname, libName) != 0) {
    return false;
  }

  if (!dynStrsTable ||
      !dladdr(dynStrsTable, &info) ||
      strcmp(info.dli_fname, libName) != 0) {
    return false;
  }

  return true;
}

} } // namespace facebook::linker
