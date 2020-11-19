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
#include <profilo/mmapbuf/MmapBufferManager.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {

class MmapBufferManagerTestAccessor {
 public:
  explicit MmapBufferManagerTestAccessor(MmapBufferManager& bufferManager)
      : bufferManager_(bufferManager) {}

  void* mmapBufferPointer() {
    return bufferManager_.buffer_->buffer;
  }

  void* mmapPointer() {
    return reinterpret_cast<void*>(bufferManager_.buffer_->prefix);
  }

  uint32_t size() {
    return bufferManager_.buffer_->totalByteSize;
  }

  TraceBuffer& ringBuffer() {
    return bufferManager_.buffer_->ringBuffer();
  }

 private:
  MmapBufferManager& bufferManager_;
};
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
