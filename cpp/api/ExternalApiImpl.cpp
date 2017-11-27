// Copyright 2004-present Facebook. All Rights Reserved.

#include <loom/ExternalApi.h>

#include <unistd.h>

#include <loom/Logger.h>
#include <loom/LogEntry.h>
#include <loom/TraceProviders.h>

using namespace facebook::loom;

void loom_internal_mark_start(uint32_t provider, const char* msg);
void loom_internal_mark_end(uint32_t provider);

#ifdef __cplusplus
extern "C" {
#endif

#define LOOM_EXPORT __attribute__((visibility ("default")))

LOOM_EXPORT LoomApi loom_api_int {
  .mark_start = &loom_internal_mark_start,
  .mark_end = &loom_internal_mark_end
};

#ifdef __cplusplus
} // extern "C"
#endif

void loom_internal_mark_start(uint32_t provider, const char* msg) {
  if (!TraceProviders::get().isEnabled(provider) || msg == nullptr) {
    return;
  }
  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::MARK_PUSH;
  int32_t id = logger.write(std::move(entry));
  size_t msg_len = strlen(msg);
  static const char* kNameKey = "__name";
  static const int kNameLen = strlen(kNameKey);

  if (msg_len > 0) {
    id = logger.writeBytes(
      entries::STRING_KEY,
      id,
      (const uint8_t *)kNameKey,
      kNameLen);
    logger.writeBytes(
      entries::STRING_VALUE,
      id,
      (const uint8_t *)msg,
      msg_len);
  }
}

void loom_internal_mark_end(uint32_t provider) {
  if (!TraceProviders::get().isEnabled(provider)) {
    return;
  }
  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::MARK_POP;
  logger.write(std::move(entry));
}
