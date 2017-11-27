// Copyright 2004-present Facebook. All Rights Reserved.

#include "ExternalApi.h"

#include <unistd.h>

#include <util/SoUtils.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace {

LoomApi* resolve_loom_api_int() {
  return reinterpret_cast<LoomApi*>(
    facebook::loom::util::resolve_symbol("loom_api_int"));
}

LoomApi* get_loom_api_int() {
  static auto loom_api = resolve_loom_api_int();
  return loom_api;
}

void loom_api_mark_start(uint32_t provider, const char* name) {
  auto loom_api = get_loom_api_int();
  if (loom_api == nullptr) {
    return;
  }
  loom_api->mark_start(provider, name);
}

void loom_api_mark_end(uint32_t provider) {
  auto loom_api = get_loom_api_int();
  if (loom_api == nullptr) {
    return;
  }
  loom_api->mark_end(provider);
}
} // namespace anonymous

LoomApi* loom_api() {
  static LoomApi loom_api {
    .mark_start = &loom_api_mark_start,
    .mark_end = &loom_api_mark_end
  };
  return &loom_api;
}

#ifdef __cplusplus
} // extern "C"
#endif
