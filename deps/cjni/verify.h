// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <assert.h>

// The verify_dummy function stops GCC complaining about values
// computed and not used in VERIFY.
static inline void verify_dummy(bool b) {}
#ifdef NDEBUG
#define VERIFY(e) (verify_dummy(e), true)
#else
#define VERIFY(e) (assert((e)), verify_dummy(true))
#endif
