// Copyright 2004-present Facebook. All Rights Reserved.

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

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*profilo_int_mark_start)(const char* provider, const char* msg);
typedef void (*profilo_int_mark_end)(const char* provider);

typedef struct ProfiloApi {
  profilo_int_mark_start mark_start;
  profilo_int_mark_end mark_end;
} ProfiloApi;

ProfiloApi* profilo_api();

#ifdef __cplusplus
} // extern "C"
#endif
