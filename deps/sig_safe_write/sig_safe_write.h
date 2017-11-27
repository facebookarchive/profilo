// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SIGSEGV and SIGBUS safe op. Performs the  specified
 * op, but first registers sigmux based SIGSEGV and SIGBUS
 * handler that bails out in case of failure.
 *
 * The operation will receive the value of data as a parameter.
 *
 * If successful, returns 0. In case of error returns non-zero
 * value and sets errno appropriately.
 */
int
sig_safe_op(void (*op)(void* data), void* data);

/**
 * Like sig_safe_op but specifically for memory writes.
 * Writes the specified value to the target address with all the protections
 * of sig_safe_op.
 *
 * If successful, returns 0. In case of error returns non-zero
 * value and sets errno appropriately.
 */
int
sig_safe_write(void* destination, intptr_t value);

#ifdef __cplusplus
}
#endif
