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

#ifndef ART_RUNTIME_GC_SPACE_MEMORY_TOOL_SETTINGS_H_
#define ART_RUNTIME_GC_SPACE_MEMORY_TOOL_SETTINGS_H_

namespace art {
namespace gc {
namespace space {

// Default number of bytes to use as a red zone (rdz). A red zone of this size will be placed before
// and after each allocation. 8 bytes provides long/double alignment.
static constexpr size_t kDefaultMemoryToolRedZoneBytes = 8;

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_MEMORY_TOOL_SETTINGS_H_
