// Copyright 2004-present Facebook. All Rights Reserved.

#include <time.h>

__attribute__((visibility("default"))) clock_t call_clock() {
  return clock();
}
