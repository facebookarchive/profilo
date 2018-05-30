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

/**
 * The API enables logging into Profilo.
 *
 * It allows logging a code block into Profilo wrapping it with "mark_start" and
 * "mark_end" functions. Provider name must be specified to each api call.
 *
 * There are two ways of using the API:
 *
 * 1. Direct usage example:
 *
 *   #include <profilo/ExternalApi.h>
 *
 *   profilo_api()->mark_start("provider_name", "my_function");
 *   ...
 *   profilo_api()->mark_end("provider_name");
 *
 *
 * 2. Using Fbsystrace:
 *
 *   #include "fbsystrace_cplusplus.h"
 *
 *   void my_function() {
 *     FbSystraceSection s(TRACE_TAG_FOO, "my_function")
 *     ...
 *   }
 *
 * NOTE: Profilo library must be loaded prior usage for the API to work.
 */

#pragma once

#include <profilo/ExternalApiGlue.h>

#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

ProfiloApi* profilo_api();

#ifdef __cplusplus
} // extern "C"
#endif
