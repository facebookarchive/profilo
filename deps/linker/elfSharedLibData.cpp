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
#include <sig_safe_write/sig_safe_write.h>

#include <stdlib.h>
#include <dlfcn.h>
#include <stdexcept>
#include <string.h>
#include <android/log.h>

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

// data_ fields are explicitly set to nullptr/0 as a sentinel to valid
elfSharedLibData::elfSharedLibData(): data_{}, parsed_state_{} {}

elfSharedLibData::elfSharedLibData(
    ElfW(Addr) addr,
    const char* name,
    const ElfW(Phdr) * phdrs,
    ElfW(Half) phnum)
    : data_{
          .loadBias = addr,
          .name = name == nullptr ? std::string() : name,
          .phdrs = phdrs,
          .phnum = phnum,
      }, parsed_state_{} {
}

bool elfSharedLibData::parse_input() {
  if (parsed_state_.attempted) {
    return parsed_state_.successful;
  }

  parsed_state_.attempted = true;
  parsed_state_.successful = false;

  ElfW(Dyn) const* dynamic_table = nullptr;
  for (int i = 0; i < data_.phnum; ++i) {
    ElfW(Phdr) const* phdr = &data_.phdrs[i];
    if (phdr->p_type == PT_DYNAMIC) {
      dynamic_table = reinterpret_cast<ElfW(Dyn) const*>(data_.loadBias + phdr->p_vaddr);
      break;
    }
  }

  if (!dynamic_table) {
    return false;
  }

  ElfW(Word) sonameOffset = 0;

  for (ElfW(Dyn) const* entry = dynamic_table; entry && entry->d_tag != DT_NULL; ++entry) {
    switch (entry->d_tag) {
      case DT_SONAME:
        sonameOffset = entry->d_un.d_val;
        break;
      case DT_PLTRELSZ:
        parsed_state_.pltRelocationsLen = entry->d_un.d_val / sizeof(Elf_Reloc);
        break;

      case DT_JMPREL: // DT_PLTREL just declares the Rel/Rela type in use, not the table data
        parsed_state_.pltRelocations =
            reinterpret_cast<Elf_Reloc const*>(data_.loadBias + entry->d_un.d_ptr);
        break;

      case DT_RELSZ:
      case DT_RELASZ:
        // bionic's linker already handles sanity checking/blowing up if wrong Rel/Rela match
        parsed_state_.relocationsLen = entry->d_un.d_val / sizeof(Elf_Reloc);
        break;

      case DT_REL:
      case DT_RELA:
        // bionic's linker already handles sanity checking/blowing up if wrong Rel/Rela match
        parsed_state_.relocations =
            reinterpret_cast<Elf_Reloc const*>(data_.loadBias + entry->d_un.d_ptr);
        break;

        // TODO (t30088113): handle DT_ANDROID_REL[A][SZ]

      case DT_SYMTAB:
        parsed_state_.dynSymbolsTable =
            reinterpret_cast<ElfW(Sym) const*>(data_.loadBias + entry->d_un.d_ptr);
        break;

      case DT_STRTAB:
        parsed_state_.dynStrsTable =
            reinterpret_cast<char const*>(data_.loadBias + entry->d_un.d_ptr);
        break;

      case DT_HASH:
        parsed_state_.elfHash.numbuckets = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr)[0];
        parsed_state_.elfHash.numchains = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr)[1];
        parsed_state_.elfHash.buckets = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr + 8);
        // chains_ is stored immediately after buckets_ and is the same type, so the index after
        // the last valid bucket value is the first valid chain value.
        parsed_state_.elfHash.chains = &parsed_state_.elfHash.buckets[parsed_state_.elfHash.numbuckets];
        break;

      case DT_GNU_HASH: // see http://www.linker-aliens.org/blogs/ali/entry/gnu_hash_elf_sections/
        // the original AOSP code uses several binary-math optimizations that differ from the "standard"
        // gnu hash implementation, and have been left in place with explanatory comments to avoid diverging
        parsed_state_.gnuHash.numbuckets = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr)[0];
        parsed_state_.gnuHash.symoffset = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr)[1];
        parsed_state_.gnuHash.bloom_size = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr)[2];
        parsed_state_.gnuHash.bloom_shift = reinterpret_cast<uint32_t const*>(data_.loadBias + entry->d_un.d_ptr)[3];
        parsed_state_.gnuHash.bloom_filter = reinterpret_cast<ElfW(Addr) const*>(data_.loadBias + entry->d_un.d_ptr + 16);
        parsed_state_.gnuHash.buckets = reinterpret_cast<uint32_t const*>(&parsed_state_.gnuHash.bloom_filter[parsed_state_.gnuHash.bloom_size]);

        // chains is stored immediately after buckets and is the same type, so the index after
        // the last valid bucket value is the first valid chain value.
        // However, note that we subtract symoffset (and thus actually start the chains array
        // INSIDE the buckets array)! This is because the chains index for a symbol is negatively
        // offset from its dynSymbolsTable index by symoffset. Normally, once you find a match in
        // chains you'd add symoffset and then you'd have your dynSymbolsTable index... but by
        // "shifting" the array backwards we can make the chains indices line up exactly with
        // dynSymbolsTable right from the start.
        // We don't have to ever worry about indexing into invalid chains data, because the
        // chain-start indices that live in buckets are indices into dynSymbolsTable and will thus
        // also never be less than symoffset.
        parsed_state_.gnuHash.chains = parsed_state_.gnuHash.buckets + parsed_state_.gnuHash.numbuckets - parsed_state_.gnuHash.symoffset;

        // verify that bloom_size_ is a power of 2
        if ((((uint32_t)(parsed_state_.gnuHash.bloom_size - 1)) & parsed_state_.gnuHash.bloom_size) != 0) {
          // shouldn't be possible; the android linker has already performed this check
          __android_log_print(ANDROID_LOG_WARN, "elfSharedLibData", "bloom_size_ not power of 2");
          return false;
        }
        // since we know that bloom_size_ is a power of two, we can simplify modulus division later in
        // gnu_find_symbol_by_name by decrementing by 1 here and then using logical-AND instead of mod-div
        // in the lookup (0x100 - 1 == 0x0ff, 0x1c3 & 0x0ff == 0x0c3.. the "remainder")
        parsed_state_.gnuHash.bloom_size--;
        break;
    }
  }
  if (!is_complete()) {
    return false;
  }

  // Verify the soname
  auto parsedSoname = &parsed_state_.dynStrsTable[sonameOffset];
  if (strcmp(parsedSoname, data_.name.c_str()) != 0) {
    __android_log_print(ANDROID_LOG_WARN, "elfSharedLibData", "Name mismatch: %s vs %s", parsedSoname, data_.name.c_str());
    return false;
  }

  parsed_state_.successful = true;
  return true;
}

bool elfSharedLibData::usesGnuHashTable() {
  if (!parsed_state_.attempted) {
    throw std::invalid_argument("Check valid() first!");
  }
  return parsed_state_.gnuHash.numbuckets > 0;
}

ElfW(Sym) const* elfSharedLibData::find_symbol_by_name(char const* name) {
  if (!parsed_state_.attempted) {
    throw std::invalid_argument("Check valid() first!");
  }

  ElfW(Sym) const* sym = nullptr;
  if (parsed_state_.gnuHash.numbuckets > 0) {
    sym = gnu_find_symbol_by_name(name);
  }
  if (!sym && parsed_state_.elfHash.numbuckets > 0) {
    sym = elf_find_symbol_by_name(name);
  }

  // the GNU hash table doesn't include entries for any STN_UNDEF symbols in the object,
  // and although the ELF hash table "should" according to the spec contain entries for
  // every symbol, there may be some noncompliant binaries out in the world.

  // I *think* that if a symbol is STN_UNDEF, it's "extern"-linked and will thus have an
  // entry in either the DT_JMPREL[A] or DT_REL[A] sections. Not entirely sure though.

  // note the !sym check in each loop: don't perform this work if we've already found it.
  for (size_t i = 0; !sym && i < parsed_state_.pltRelocationsLen; i++) {
    size_t sym_idx = ELF_RELOC_SYM(parsed_state_.pltRelocations[i].r_info);
    if (strcmp(parsed_state_.dynStrsTable + parsed_state_.dynSymbolsTable[sym_idx].st_name, name) == 0) {
      sym = &parsed_state_.dynSymbolsTable[sym_idx];
    }
  }
  for (size_t i = 0; !sym && i < parsed_state_.relocationsLen; i++) {
    size_t sym_idx = ELF_RELOC_SYM(parsed_state_.relocations[i].r_info);
    if (strcmp(parsed_state_.dynStrsTable + parsed_state_.dynSymbolsTable[sym_idx].st_name, name) == 0) {
      sym = &parsed_state_.dynSymbolsTable[sym_idx];
    }
  }

  return sym;
}

ElfW(Sym) const* elfSharedLibData::elf_find_symbol_by_name(char const* name) const {
  uint32_t hash = elfhash(name);

  for (uint32_t n = parsed_state_.elfHash.buckets[hash % parsed_state_.elfHash.numbuckets]; n != 0; n = parsed_state_.elfHash.chains[n]) {
    ElfW(Sym) const* s = parsed_state_.dynSymbolsTable + n; // identical to &parsed_state_.dynSymbolsTable[n]
    if (strcmp(parsed_state_.dynStrsTable + s->st_name, name) == 0) {
      return s;
    }
  }

  return nullptr;
}

// the original AOSP code uses several binary-math optimizations that differ from the "standard"
// gnu hash implementation, and have been left in place with explanatory comments to avoid diverging
ElfW(Sym) const* elfSharedLibData::gnu_find_symbol_by_name(char const* name) const {
  uint32_t hash = gnuhash(name);
  uint32_t h2 = hash >> parsed_state_.gnuHash.bloom_shift;

  // bloom_size_ has been decremented by 1 from its original value (which was guaranteed
  // to be a power of two), meaning that this is mathematically equivalent to modulus division:
  // 0x100 - 1 == 0x0ff, and 0x1c3 & 0x0ff = 0x0c3.. the "remainder"
  uint32_t word_num = (hash / kBloomMaskBits) & parsed_state_.gnuHash.bloom_size;
  ElfW(Addr) bloom_word = parsed_state_.gnuHash.bloom_filter[word_num];

  // test against bloom filter
  if ((1 & (bloom_word >> (hash % kBloomMaskBits)) & (bloom_word >> (h2 % kBloomMaskBits))) == 0) {
    return nullptr;
  }

  // bloom test says "probably yes"...
  uint32_t n = parsed_state_.gnuHash.buckets[hash % parsed_state_.gnuHash.numbuckets];

  if (n == 0) {
    return nullptr;
  }

  do {
    ElfW(Sym) const* s = parsed_state_.dynSymbolsTable + n; // identical to &parsed_state_.dynSymbolsTable[n]
    // this XOR is mathematically equivalent to (hash1 | 1) == (hash2 | 1), but faster
    // basically, test for equality while ignoring LSB
    if (((parsed_state_.gnuHash.chains[n] ^ hash) >> 1) == 0 &&
        strcmp(parsed_state_.dynStrsTable + s->st_name, name) == 0) {
      return s;
    }
  } while ((parsed_state_.gnuHash.chains[n++] & 1) == 0); // gnu hash chains use the LSB as an end-of-chain marker

  return nullptr;
}

std::vector<void**> elfSharedLibData::get_relocations(void *symbol) {
  if (!parsed_state_.attempted) {
    throw std::invalid_argument("Check valid() first!");
  }

  std::vector<void**> relocs;

  for (size_t i = 0; i < parsed_state_.relocationsLen; i++) {
    auto const& relocation = parsed_state_.relocations[i];
    void** relocated = reinterpret_cast<void**>(data_.loadBias + relocation.r_offset);
    if (*relocated == symbol) {
      relocs.push_back(relocated);
    }
  }

  return relocs;
}

std::vector<void**> elfSharedLibData::get_plt_relocations(ElfW(Sym) const* elf_sym) {
  if (!parsed_state_.attempted) {
    throw std::invalid_argument("Check valid() first!");
  }

  // TODO: merge this and get_relocations into one helper-based implementation
  std::vector<void**> relocs;

  for (unsigned int i = 0; i < parsed_state_.pltRelocationsLen; i++) {
    Elf_Reloc const& rel = parsed_state_.pltRelocations[i];

    // is this necessary? will there ever be a type of relocation in parsed_state_.pltRelocations
    // that points to symbol and we *don't* want to fix up?
    if (ELF_RELOC_TYPE(rel.r_info) != PLT_RELOCATION_TYPE) {
      continue;
    }

    if (&parsed_state_.dynSymbolsTable[ELF_RELOC_SYM(rel.r_info)] == elf_sym) {
      // Found the address of the relocation
      void** plt_got_entry = reinterpret_cast<void**>(data_.loadBias + rel.r_offset);
      relocs.push_back(plt_got_entry);
    }
  }

  return relocs;
}

std::vector<void**> elfSharedLibData::get_plt_relocations(void const* addr) {
  if (!parsed_state_.attempted) {
    throw std::invalid_argument("Check valid() first!");
  }

  std::vector<void**> relocs;

  for (unsigned int i = 0; i < parsed_state_.pltRelocationsLen; i++) {
    Elf_Reloc const& rel = parsed_state_.pltRelocations[i];

    // is this necessary? will there ever be a type of relocation in parsed_state_.pltRelocations
    // that points to symbol and we *don't* want to fix up?
    if (ELF_RELOC_TYPE(rel.r_info) != PLT_RELOCATION_TYPE) {
      continue;
    }
    void** plt_got_entry = reinterpret_cast<void**>(data_.loadBias + rel.r_offset);
    if (*plt_got_entry == addr) {
      relocs.push_back(plt_got_entry);
    }
  }

  return relocs;
}

bool elfSharedLibData::is_complete() const {
  return parsed_state_.pltRelocationsLen && parsed_state_.pltRelocations &&
         // parsed_state_.relocationsLen && parsed_state_.relocations &&     TODO (t30088113): re-enable when DT_ANDROID_REL is supported
         parsed_state_.dynSymbolsTable && parsed_state_.dynStrsTable &&
         (parsed_state_.elfHash.numbuckets > 0 || parsed_state_.gnuHash.numbuckets > 0);
}

/* It can happen that, after caching a shared object's data in sharedLibData,
 * the library is unloaded, so references to memory in that address space
 * result in SIGSEGVs. Thus, check here that the base address is still valid via
 * by reading from it and ensuring it holds the same bytes it did initially.
 */
bool elfSharedLibData::valid() {
  if (data_.loadBias == 0 && data_.phdrs == nullptr &&
      data_.phnum == 0) {
    // Default constructed object
    return false;
  }

  // Try and parse the library but fail safely if it's unmapped
  struct parse_result {
    elfSharedLibData* data;
    bool result;
  } state {
    .data = this,
    .result = false,
  };

  // Force a reparse.
  parsed_state_.attempted = false;

  if (sig_safe_op([](void* data){
    parse_result* state = (parse_result*) data;
    state->result = state->data->parse_input();
  }, &state)) {
    return false;
  }

  if (!state.result) {
    return false;
  }

  // Parse the inputs if necessary.
  if (!parse_input()) {
    return false;
  }

  return true;
}

} } // namespace facebook::linker
