// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdlib.h>
#include <inttypes.h>

typedef void* mspace;

static inline size_t
mspace_usable_size(const void* mem) {
  abort();
}

static inline void*
mspace_malloc(mspace msp, size_t bytes) {
  abort();
}
