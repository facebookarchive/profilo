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

#ifdef __cplusplus
extern "C" {
#endif

#define __ABORT_MESSAGE_PREFIX(file, line) file ":" line " "
#define __TO_STR_INDIRECTED(x) #x
#define __TO_STR(x) __TO_STR_INDIRECTED(x)

#define abortWithReason(msg) \
  abortWithReasonImpl(__ABORT_MESSAGE_PREFIX(__FILE__, __TO_STR(__LINE__)) msg)

__attribute__((noreturn)) void abortWithReasonImpl(const char* reason);

#ifdef __cplusplus
} // extern "C"
#endif
