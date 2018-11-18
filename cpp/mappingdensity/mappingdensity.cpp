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

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <fb/log.h>
#include <mappingdensity/mappingdensity.h>
#include <procmaps.h>

namespace facebook {
namespace profilo {
namespace mappingdensity {

void dumpMappingDensities(
    std::string const& mapRegexStr,
    std::string const& outFile) {
  struct memorymap* maps = memorymap_snapshot(getpid());
  if (maps == nullptr) {
    FBLOGE("failed to take memorymap snapshot: %s", std::strerror(errno));
    return;
  }

  std::regex mapRegex(mapRegexStr);

  size_t maxSize = 0;
  for (struct memorymap_vma const* vma = memorymap_first_vma(maps);
       vma != nullptr;
       vma = memorymap_vma_next(vma)) {
    if (!std::regex_search(memorymap_vma_file(vma) ?: "", mapRegex)) {
      continue;
    }

    size_t sz = memorymap_vma_end(vma) - memorymap_vma_start(vma);
    if (sz > maxSize) {
      maxSize = sz;
    }
  }

  size_t const pageSize = sysconf(_SC_PAGESIZE);
  std::vector<uint8_t> buf(maxSize / pageSize + 1);
  std::ofstream os(outFile);
  for (struct memorymap_vma const* vma = memorymap_first_vma(maps);
       vma != nullptr;
       vma = memorymap_vma_next(vma)) {
    if (!std::regex_search(memorymap_vma_file(vma) ?: "", mapRegex)) {
      continue;
    }

    int ret = mincore(
        reinterpret_cast<void*>(memorymap_vma_start(vma)),
        memorymap_vma_end(vma) - memorymap_vma_start(vma),
        buf.data());
    if (ret != 0) {
      FBLOGW(
          "failed to get mincore for %s: %s",
          memorymap_vma_file(vma),
          std::strerror(errno));
    }

    os << memorymap_vma_file(vma) << std::ends;
    os << memorymap_vma_permissions(vma) << std::ends;
    uint64_t value;
    value = memorymap_vma_start(vma);
    os.write(reinterpret_cast<char const*>(&value), sizeof(value));
    value = memorymap_vma_end(vma);
    os.write(reinterpret_cast<char const*>(&value), sizeof(value));
    value = (memorymap_vma_end(vma) - memorymap_vma_start(vma)) / pageSize + 1;
    if (ret != 0) {
      value = 0;
    }
    os.write(reinterpret_cast<char const*>(&value), sizeof(value));
    os.write(reinterpret_cast<char const*>(buf.data()), value);

    if (!os) {
      FBLOGE(
          "failed to write mapping data for %s to %s: %s",
          memorymap_vma_file(vma),
          outFile.c_str(),
          std::strerror(errno));
      unlink(outFile.c_str());
      break;
    }
  }

  memorymap_destroy(maps);
}

} // namespace mappingdensity
} // namespace profilo
} // namespace facebook
