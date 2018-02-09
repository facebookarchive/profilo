// Copyright 2004-present Facebook. All Rights Reserved.

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
