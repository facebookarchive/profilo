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

#include <cstring>

#include <yarn/Event.h>
#include <yarn/Records.h>
#include <yarn/detail/Reader.h>

namespace facebook {
namespace yarn {
namespace detail {
namespace parser {

using IdEventMap = std::unordered_map<uint64_t, const Event&>;

void parseBuffer(
    const Event& bufferEvent,
    IdEventMap& idEventMap,
    RecordListener* listener);

} // namespace parser
} // namespace detail
} // namespace yarn
} // namespace facebook
