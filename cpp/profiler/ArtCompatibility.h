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

namespace facebook {
namespace profilo {
namespace artcompat {

#if defined(MUSEUM_VERSION_5_1_1)
void registerNatives_5_1_1();
#endif
#if defined(MUSEUM_VERSION_6_0_1)
void registerNatives_6_0_1();
#endif
#if defined(MUSEUM_VERSION_7_0_0)
void registerNatives_7_0_0();
#endif

} // artcompat
} // profilo
} // facebook
