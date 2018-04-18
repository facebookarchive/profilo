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
 * SIGILL safe op. Performs the specified op, but first registers sigmux based
 * SIGILL handler that bails out in case of failure.
 *
 * The operation will receive the value of data as a parameter.
 *
 * If successful, returns 0. In case of error returns non-zero
 * value and sets errno appropriately.
 */
int
sig_safe_exec(void (*op)(void* data), void* data);

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
