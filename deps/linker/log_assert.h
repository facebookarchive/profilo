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

#ifdef __ANDROID__

#include <android/log.h>
#include <stdlib.h>

#ifdef LOG_TAG
#define LINKER_ASSERT_LOG_TAG LOG_TAG
#else
#define LINKER_ASSERT_LOG_TAG "linkerlib"
#endif

__attribute__ ((noreturn))
static void log_assert(char const* msg) {
  __android_log_assert("", LINKER_ASSERT_LOG_TAG, "%s", msg);
  abort(); // just for good measure
}

#else

#include <stdio.h>
#include <stdlib.h>

__attribute__ ((noreturn))
static void log_assert(char const* msg) {
  fputs("Assertion Failure: ", stderr);
  fputs(msg, stderr);
  fputc('\n', stderr);
  abort();
}

#endif
