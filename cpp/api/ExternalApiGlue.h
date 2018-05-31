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

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*profilo_int_mark_start)(const char* provider, const char* msg);
typedef void (*profilo_int_mark_end)(const char* provider);
typedef void (*profilo_int_log_classload_start)(const char* provider);
typedef void (*profilo_int_log_classload_end)(const char* provider, int64_t classid);
typedef void (*profilo_int_log_classload_failed)(const char* provider);

typedef struct ProfiloApi {
  profilo_int_mark_start mark_start;
  profilo_int_mark_end mark_end;
  profilo_int_log_classload_start log_classload_start;
  profilo_int_log_classload_end log_classload_end;
  profilo_int_log_classload_failed log_classload_failed;
} ProfiloApi;

extern ProfiloApi profilo_api_int;

#ifdef __cplusplus
} // extern "C"
#endif
