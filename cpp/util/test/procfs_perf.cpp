// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include <unistd.h>
#include <sys/types.h>

#include <util/ProcFs.h>

using namespace facebook::loom;

int main() {

  util::TaskStatFile file{(uint32_t) getpid()};

  for (int i = 0; i < 1000000; i++) {
    auto info = file.refresh();
    (void) info;
    //std::cout << info.state << ' ' << info.cpuTime << '\n';
  }
  return 0;
}
