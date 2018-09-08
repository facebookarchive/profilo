/**
 * Copyright 2018-present, Facebook, Inc.
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

#ifndef FBASSERT_H
#define FBASSERT_H

#include <fb/visibility.h>

namespace facebook {
#define ENABLE_FBASSERT 1

#if ENABLE_FBASSERT
#define FBASSERTMSGF(expr, msg, ...)                                       \
  !(expr) ? facebook::assertInternal(                                      \
                "Assert (%s:%d): " msg, __FILE__, __LINE__, ##__VA_ARGS__) \
          : (void)0
#else
#define FBASSERTMSGF(expr, msg, ...)
#endif // ENABLE_FBASSERT

#define FBASSERT(expr) FBASSERTMSGF(expr, "%s", #expr)

#define FBCRASH(msg, ...)   \
  facebook::assertInternal( \
      "Fatal error (%s:%d): " msg, __FILE__, __LINE__, ##__VA_ARGS__)
#define FBUNREACHABLE()     \
  facebook::assertInternal( \
      "This code should be unreachable (%s:%d)", __FILE__, __LINE__)

FBEXPORT void assertInternal(const char* formatstr, ...)
    __attribute__((noreturn));

// This allows storing the assert message before the current process terminates
// due to a crash
typedef void (*AssertHandler)(const char* message);
void setAssertHandler(AssertHandler assertHandler);

} // namespace facebook
#endif // FBASSERT_H
