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

#include <string.h>
#include <sys/mman.h>
#include <new>

#include <forkjail/ShmErrorMsg.h>

namespace facebook {
namespace forkjail {

ShmErrorMsg::ShmErrorMsg()
  : map_(reinterpret_cast<char*>(
      mmap(nullptr, kPageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0))) {
  if (!map_) {
    throw std::bad_alloc();
  }
}

ShmErrorMsg::~ShmErrorMsg() {
  munmap(map_, kPageSize);
}

void ShmErrorMsg::set(char const* msg) {
  strncpy(map_, msg, kPageSize);
  map_[kPageSize - 1] = 0;
}

} } // namespace facebook::forkjail
