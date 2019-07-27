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

#include <cstring>

#include <fb/log.h>
#include <procmaps.h>
#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <profilo/mappings/mappings.h>

#include <sys/types.h>
#include <unistd.h>
#include <sstream>

namespace facebook {
namespace profilo {
namespace mappings {

namespace {

bool starts_with(
    std::string const& haystack,
    const char* needle,
    size_t needle_sz) {
  if (haystack.size() < needle_sz) {
    return false;
  }
  return std::equal(
      haystack.begin(),
      haystack.begin() + needle_sz,
      needle,
      needle + needle_sz);
}

bool ends_with(
    std::string const& haystack,
    const char* needle,
    size_t needle_sz) {
  if (haystack.size() < needle_sz) {
    return false;
  }
  return std::equal(
      haystack.rbegin(),
      haystack.rbegin() + needle_sz,
      std::reverse_iterator<const char*>(needle + needle_sz),
      std::reverse_iterator<const char*>(needle));
}

} // namespace

/* Log only interesting file-backed memory mappings. */
void logMemoryMappings(JNIEnv*, jobject) {
  auto memorymap = memorymap_snapshot(getpid());
  if (memorymap == nullptr) {
    FBLOGE("Could not read memory mappings");
    return;
  }

  auto& logger = Logger::get();
  auto tid = threadID();
  auto time = monotonicTime();

  FBLOGV("Num mappings: %zu", memorymap_size(memorymap));

  std::stringstream stream;
  stream << std::hex;

  static constexpr char kAndroidMappingKey[] = "s:e:o:f";

  for (auto vma = memorymap_first_vma(memorymap); vma != nullptr;
       vma = memorymap_vma_next(vma)) {
    auto file = memorymap_vma_file(vma);
    if (file == nullptr || strlen(file) == 0 || strcmp(file, " ") == 0) {
      // We need to have a path.
      continue;
    }
    std::string filestr(file);
    // Accept any .so and all files under the whitelisted folders.
    static constexpr char kSystemLib[] = "/system/lib";
    static constexpr char kSystemBin[] = "/system/bin";
    static constexpr char kSystemFramework[] = "/system/framework";
    static constexpr char kData[] = "/data";
    bool whitelisted_file =
        // short string checks first
        ends_with(filestr, ".so", 3) || ends_with(filestr, ".odex", 5) ||
        ends_with(filestr, ".oat", 4) || ends_with(filestr, ".art", 4) ||
        ends_with(filestr, ".jar", 4) ||
        starts_with(filestr, kData, sizeof(kData) - 1) ||
        starts_with(filestr, kSystemLib, sizeof(kSystemLib) - 1) ||
        starts_with(filestr, kSystemBin, sizeof(kSystemBin) - 1) ||
        starts_with(filestr, kSystemFramework, sizeof(kSystemFramework) - 1);

    if (!whitelisted_file) {
      continue;
    }

    stream.str(std::string());
    stream.clear();

    stream << memorymap_vma_start(vma) << ":" << memorymap_vma_end(vma) << ":"
           << memorymap_vma_offset(vma) << ":" << memorymap_vma_file(vma);

    auto formatted_entry = stream.str();

    FBLOGV("Logging mapping: %s", formatted_entry.c_str());

    auto mappingId = logger.write(StandardEntry{
        .type = entries::MAPPING,
        .tid = tid,
        .timestamp = time,
    });
    auto keyId = logger.writeBytes(
        entries::STRING_KEY,
        mappingId,
        reinterpret_cast<const uint8_t*>(kAndroidMappingKey),
        sizeof(kAndroidMappingKey));
    logger.writeBytes(
        entries::STRING_VALUE,
        keyId,
        reinterpret_cast<const uint8_t*>(formatted_entry.data()),
        formatted_entry.size());
  }

  memorymap_destroy(memorymap);
}

} // namespace mappings
} // namespace profilo
} // namespace facebook
