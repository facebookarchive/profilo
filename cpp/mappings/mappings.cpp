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
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mappings/mappings.h>
#include <util/common.h>

#include <sys/types.h>
#include <unistd.h>
#include <sstream>

namespace facebook {
namespace profilo {
namespace mappings {

/* Log only interesting file-backed memory mappings. */
void logMemoryMappings(JNIEnv*, jobject) {
  auto memorymap = memorymap_snapshot(getpid());
  if (memorymap == nullptr) {
    FBLOGE("Could not read memory mappings");
    return;
  }

  auto& logger = RingBuffer::get().logger();
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

    stream.str(std::string());
    stream.clear();

    stream << memorymap_vma_start(vma) << ":" << memorymap_vma_end(vma) << ":"
           << memorymap_vma_offset(vma) << ":" << memorymap_vma_file(vma);

    auto formatted_entry = stream.str();

    FBLOGV("Logging mapping: %s", formatted_entry.c_str());

    auto mappingId = logger.write(StandardEntry{
        .type = EntryType::MAPPING,
        .timestamp = time,
        .tid = tid,
    });
    auto keyId = logger.writeBytes(
        EntryType::STRING_KEY,
        mappingId,
        reinterpret_cast<const uint8_t*>(kAndroidMappingKey),
        sizeof(kAndroidMappingKey));
    logger.writeBytes(
        EntryType::STRING_VALUE,
        keyId,
        reinterpret_cast<const uint8_t*>(formatted_entry.data()),
        formatted_entry.size());
  }

  memorymap_destroy(memorymap);
}

} // namespace mappings
} // namespace profilo
} // namespace facebook
