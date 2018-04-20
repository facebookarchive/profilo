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

#include "ExternalApi.h"

#include <unistd.h>

#include <util/SoUtils.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace {

ProfiloApi* resolve_api_int() {
  return reinterpret_cast<ProfiloApi*>(
    facebook::profilo::util::resolve_symbol("profilo_api_int"));
}

ProfiloApi* get_api_int() {
  static auto api = resolve_api_int();
  return api;
}

void api_mark_start(const char* provider, const char* name) {
  auto api = get_api_int();
  if (api == nullptr) {
    return;
  }
  api->mark_start(provider, name);
}

void api_mark_end(const char* provider) {
  auto api = get_api_int();
  if (api == nullptr) {
    return;
  }
  api->mark_end(provider);
}

void api_log_classload(const char* provider, int64_t classid) {
  auto api = get_api_int();
  if (api == nullptr) {
    return;
  }
  api->log_classload(provider, classid);
}

} // namespace anonymous

ProfiloApi* profilo_api() {
  static ProfiloApi api {
    .mark_start = &api_mark_start,
    .mark_end = &api_mark_end,
    .log_classload = &api_log_classload,
  };
  return &api;
}

#ifdef __cplusplus
} // extern "C"
#endif
