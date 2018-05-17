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

#include <linker/elfSharedLibData.h>

#include <string>
#include <vector>

namespace facebook {
namespace linker {

/**
 * Looks up an elfSharedLibData by name.
 */
elfSharedLibData sharedLib(char const*);

/**
 * Returns a vector of copies of all known elfSharedLibData objects at this moment.
 */
std::vector<std::pair<std::string, elfSharedLibData>> allSharedLibs();

} } // namespace facebook::linker

extern "C" {

/**
 * Learns about all shared libraries in the process and creates elfSharedLibData
 * entries for any we don't already know of.
 */
int refresh_shared_libs();

} // extern "C"
