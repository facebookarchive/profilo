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

#include <procmaps.h>
#include <profilo/systemcounters/MappingAggregator.h>

namespace facebook {
namespace profilo {
namespace counters {

namespace {
struct MemorymapSnapshot {
  MemorymapSnapshot(struct memorymap* vma) : vma_(vma) {}
  ~MemorymapSnapshot() {
    if (vma_ != nullptr) {
      memorymap_destroy(vma_);
    }
  }

  struct memorymap* vma_;
};
} // namespace

bool MappingAggregator::refresh() {
  gl_dev_ = dmabuf_ = -1;

  auto* snapshot = memorymap_snapshot(getpid());
  if (!snapshot) {
    return false;
  }
  MemorymapSnapshot raiisnapshot{snapshot};

  gl_dev_ = dmabuf_ = 0;

  for (auto* vma = memorymap_first_vma(snapshot); vma != nullptr;
       vma = memorymap_vma_next(vma)) {
    auto* file = memorymap_vma_file(vma);
    if (file == nullptr) {
      continue;
    }

    auto size = memorymap_vma_end(vma) - memorymap_vma_start(vma);

    constexpr static char kDevKgsl[] = "/dev/kgsl-3d0";
    constexpr static char kAnonInodeDmabuf[] = "anon_inode:dmabuf";
    if (strncmp(file, kDevKgsl, strlen(kDevKgsl)) == 0) {
      gl_dev_ += size;
    } else if (strncmp(file, kAnonInodeDmabuf, strlen(kAnonInodeDmabuf)) == 0) {
      dmabuf_ += size;
    }
  }
  return true;
}

ssize_t MappingAggregator::getGLDevSize() {
  return gl_dev_;
}
ssize_t MappingAggregator::getDmabufSize() {
  return dmabuf_;
}

} // namespace counters
} // namespace profilo
} // namespace facebook
