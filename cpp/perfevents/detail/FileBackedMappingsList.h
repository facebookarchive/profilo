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

#include <procmaps.h>
#include <cstring>
#include <map>

namespace facebook {
namespace perfevents {
namespace detail {

struct FileBackedMappingsList {
  struct Mapping {
    uint64_t start;
    uint64_t end;
  };

  void fillFromProcMaps() {
    auto memorymap = memorymap_snapshot(getpid());
    if (memorymap == nullptr) {
      return;
    }
    for (auto vma = memorymap_first_vma(memorymap); vma != nullptr;
         vma = memorymap_vma_next(vma)) {
      auto file = memorymap_vma_file(vma);
      if (isAnonymous(file)) {
        continue;
      }
      add(memorymap_vma_start(vma), memorymap_vma_end(vma));
    }
    memorymap_destroy(memorymap);
  }

  void add(uint64_t start, uint64_t end) {
    const auto iter = file_mappings.lower_bound(start);
    if (iter == file_mappings.cend() || iter->second.start != start ||
        iter->second.end != end) {
      file_mappings.emplace(end, Mapping{start, end});
    }
  }

  bool contains(uint64_t addr) {
    const auto iter = file_mappings.lower_bound(addr);
    if (iter == file_mappings.cend()) {
      return false;
    }

    auto& mapping = iter->second;
    return mapping.start <= addr && addr < mapping.end;
  }

  static inline bool isAnonymous(const char* filename) {
    if (filename == nullptr || strlen(filename) == 0 ||
        strcmp(filename, " ") == 0) {
      return true;
    }

    static constexpr char kDevAshmem[] = "/dev/ashmem/";
    if (strncmp(filename, kDevAshmem, strlen(kDevAshmem)) == 0) {
      return true;
    }

    static constexpr char kBracketStack[] = "[stack";
    // e.g. "[stack:1101]" or "[stack]"
    if (strncmp(filename, kBracketStack, strlen(kBracketStack)) == 0) {
      return true;
    }

    static constexpr char kBracketAnon[] = "[anon:";
    // e.g. "[anon:linker_alloc]"
    if (strncmp(filename, kBracketAnon, strlen(kBracketAnon)) == 0) {
      return true;
    }

    static constexpr char kAnonInode[] = "anon_inode";
    // e.g. "anon_inode:[perf_event]"
    if (strncmp(filename, kAnonInode, strlen(kAnonInode)) == 0) {
      return true;
    }

    return false;
  }

 private:
  std::map<uint64_t, Mapping> file_mappings;
};

} // namespace detail
} // namespace perfevents
} // namespace facebook
