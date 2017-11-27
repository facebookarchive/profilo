/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <elf.h>
#include <link.h>
#include <stdbool.h>

#pragma once

#define R_386_JUMP_SLOT  7
#define R_ARM_JUMP_SLOT  22

#ifdef __ARM__
#define PLT_RELOCATION_TYPE R_ARM_JUMP_SLOT
#endif

#ifdef __X86__
#define PLT_RELOCATION_TYPE R_386_JUMP_SLOT
#endif

#ifndef PT_GNU_RELRO
#define PT_GNU_RELRO 0x6474e552
#endif

#define SOINFO_NAME_LEN 128
#define __work_around_b_19059885__ 1

typedef void (*linker_function_t)();

struct link_map {
  ElfW(Addr) l_addr;
  char* l_name;
  ElfW(Dyn)* l_ld;
  struct link_map* l_next;
  struct link_map* l_prev;
};
typedef struct link_map link_map;

struct soinfo;
typedef struct soinfo soinfo;

struct soinfo {
#if defined(__work_around_b_19059885__)
  char old_name_[SOINFO_NAME_LEN];
#endif
  const ElfW(Phdr)* phdr;
  size_t phnum;
  ElfW(Addr) entry;
  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_19059885__)
  uint32_t unused1;  // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn)* dynamic;

#if defined(__work_around_b_19059885__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfo* next;
  uint32_t flags_;

  const char* strtab_;
  ElfW(Sym)* symtab_;

  size_t nbucket_;
  size_t nchain_;
  uint32_t* bucket_;
  uint32_t* chain_;

#if defined(__mips__) || !defined(__LP64__)
  // This is only used by mips and mips64, but needs to be here for
  // all 32-bit architectures to preserve binary compatibility.
  ElfW(Addr)** plt_got_;
#endif

#if defined(USE_RELA)
  ElfW(Rela)* plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela)* rela_;
  size_t rela_count_;
#else
  ElfW(Rel)* plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel)* rel_;
  size_t rel_count_;
#endif

  linker_function_t* preinit_array_;
  size_t preinit_array_count_;

  linker_function_t* init_array_;
  size_t init_array_count_;
  linker_function_t* fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#if defined(__arm__)
  // ARM EABI section used for stack unwinding.
  uint32_t* ARM_exidx;
  size_t ARM_exidx_count;
#elif defined(__mips__)
  uint32_t mips_symtabno_;
  uint32_t mips_local_gotno_;
  uint32_t mips_gotsym_;

#endif
  size_t ref_count_;
  link_map link_map_head;

  _Bool constructors_called;

  // When you read a virtual address from the ELF file, add this
  // value to get the corresponding address in the process' address space.
  ElfW(Addr) load_bias;

};
