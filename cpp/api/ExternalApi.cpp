// Copyright 2004-present Facebook. All Rights Reserved.

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
} // namespace anonymous

ProfiloApi* profilo_api() {
  static ProfiloApi api {
    .mark_start = &api_mark_start,
    .mark_end = &api_mark_end
  };
  return &api;
}

#ifdef __cplusplus
} // extern "C"
#endif
