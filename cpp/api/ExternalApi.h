// Copyright 2004-present Facebook. All Rights Reserved.

/**
 * The API enables logging into Loom.
 *
 * It allows logging a code block into Loom wrapping it with "mark_start" and
 * "mark_end" functions. Provider id must be specified to each api call:
 * currently only LOOM_PROVIDER_OTHER is supported.
 *
 * There are two ways of using the API:
 *
 * 1. Direct usage example:
 *
 *   #include <loom/ExternalApi.h>
 *
 *   loom_api()->mark_start(LOOM_PROVIDER_OTHER, "my_function");
 *   ...
 *   loom_api()->mark_end(LOOM_PROVIDER_OTHER);
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
 * NOTE: Loom library must be loaded prior usage for the API to work.
 */

#pragma once

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*loom_int_mark_start)(const char* provider, const char* msg);
typedef void (*loom_int_mark_end)(const char* provider);

typedef struct LoomApi {
  loom_int_mark_start mark_start;
  loom_int_mark_end mark_end;
} LoomApi;

LoomApi* loom_api();

#ifdef __cplusplus
} // extern "C"
#endif
