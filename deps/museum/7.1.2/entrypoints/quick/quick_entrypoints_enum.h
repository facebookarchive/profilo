/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_ENUM_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_ENUM_H_

#include "quick_entrypoints.h"
#include "quick_entrypoints_enum.h"
#include "thread.h"

namespace art {

// Define an enum for the entrypoints. Names are prepended a 'kQuick'.
enum QuickEntrypointEnum
{  // NOLINT(whitespace/braces)
#define ENTRYPOINT_ENUM(name, rettype, ...) kQuick ## name,
#include "quick_entrypoints_list.h"
  QUICK_ENTRYPOINT_LIST(ENTRYPOINT_ENUM)
#undef QUICK_ENTRYPOINT_LIST
#undef ENTRYPOINT_ENUM
};

std::ostream& operator<<(std::ostream& os, const QuickEntrypointEnum& kind);

// Translate a QuickEntrypointEnum value to the corresponding ThreadOffset.
template <size_t pointer_size>
static ThreadOffset<pointer_size> GetThreadOffset(QuickEntrypointEnum trampoline) {
  switch (trampoline)
  {  // NOLINT(whitespace/braces)
  #define ENTRYPOINT_ENUM(name, rettype, ...) case kQuick ## name : \
      return QUICK_ENTRYPOINT_OFFSET(pointer_size, p ## name);
  #include "quick_entrypoints_list.h"
    QUICK_ENTRYPOINT_LIST(ENTRYPOINT_ENUM)
  #undef QUICK_ENTRYPOINT_LIST
  #undef ENTRYPOINT_ENUM
  };
  LOG(FATAL) << "Unexpected trampoline " << static_cast<int>(trampoline);
  UNREACHABLE();
}

// Do a check functions to be able to test whether the right signature is used.
template <QuickEntrypointEnum entrypoint, typename... Types>
void CheckEntrypointTypes();

#define ENTRYPOINT_ENUM(name, ...) \
template <> inline void CheckEntrypointTypes<kQuick ## name, __VA_ARGS__>() {};  // NOLINT [readability/braces] [4]
#include "quick_entrypoints_list.h"
  QUICK_ENTRYPOINT_LIST(ENTRYPOINT_ENUM)
#undef QUICK_ENTRYPOINT_LIST
#undef ENTRYPOINT_ENUM

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_ENUM_H_
