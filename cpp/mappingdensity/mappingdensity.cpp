// Copyright 2004-present Facebook. All Rights Reserved.

#include <cstring>
#include <string>
#include <fstream>
#include <regex>
#include <sys/mman.h>
#include <sys/types.h>
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
    std::string const& outFile,
    std::string const& dumpName) {
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

    size_t sz = memorymap_vma_end(vma) - memorymap_vma_end(vma);
    if (sz > maxSize) {
      maxSize = sz;
    }
  }

  size_t const pageSize = sysconf(_SC_PAGESIZE);
  std::vector<uint8_t> buf(maxSize / pageSize + 1);
  std::ofstream os(outFile + "/mincore_" + dumpName);
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
      FBLOGW("failed to get mincore for %s: %s", memorymap_vma_file(vma), std::strerror(errno));
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
      break;
    }
  }

  memorymap_destroy(maps);
}

} } } // namespace facebook::profilo::mappingdensity
