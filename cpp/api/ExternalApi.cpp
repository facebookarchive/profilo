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

#ifdef __cplusplus
extern "C" {
#endif

namespace {

void api_mark_start(const char* provider, const char* name, size_t len = 0) {
  if (profilo_api_int.mark_start == nullptr) {
    return;
  }
  profilo_api_int.mark_start(provider, name, len);
}

void api_mark_end(const char* provider) {
  if (profilo_api_int.mark_end == nullptr) {
    return;
  }
  profilo_api_int.mark_end(provider);
}

void api_log_classload_start(const char* provider) {
  if (profilo_api_int.log_classload_start == nullptr) {
    return;
  }
  profilo_api_int.log_classload_start(provider);
}

void api_log_classload_end(const char* provider, int64_t classid) {
  if (profilo_api_int.log_classload_end == nullptr) {
    return;
  }
  profilo_api_int.log_classload_end(provider, classid);
}

void api_log_classload_failed(const char* provider) {
  if (profilo_api_int.log_classload_failed == nullptr) {
    return;
  }
  profilo_api_int.log_classload_failed(provider);
}

bool api_is_enabled(const char* provider) {
  if (profilo_api_int.is_enabled == nullptr) {
    return false;
  }
  return profilo_api_int.is_enabled(provider);
}

bool api_register_external_tracer_callback(
    int tracerType,
    profilo_int_collect_stack_fn callback) {
  if (profilo_api_int.register_external_tracer_callback == nullptr) {
    return false;
  }
  return profilo_api_int.register_external_tracer_callback(
      tracerType, callback);
}

} // namespace

ProfiloApi* profilo_api() {
  static ProfiloApi api{
      .mark_start = &api_mark_start,
      .mark_end = &api_mark_end,
      .log_classload_start = &api_log_classload_start,
      .log_classload_end = &api_log_classload_end,
      .log_classload_failed = &api_log_classload_failed,
      .is_enabled = &api_is_enabled,
      .register_external_tracer_callback =
          &api_register_external_tracer_callback,
  };
  return &api;
}

#ifdef __cplusplus
} // extern "C"
#endif
