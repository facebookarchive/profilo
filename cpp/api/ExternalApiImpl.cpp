// Copyright 2004-present Facebook. All Rights Reserved.

#include <profilo/ExternalApi.h>

#include <unistd.h>

#include <profilo/Logger.h>
#include <profilo/LogEntry.h>
#include <profilo/TraceProviders.h>

using namespace facebook::profilo;

namespace {

void internal_mark_start(const char* provider, const char* msg) {
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
        entries::STRING_KEY, id, (const uint8_t*)kNameKey, kNameLen);
    logger.writeBytes(entries::STRING_VALUE, id, (const uint8_t*)msg, msg_len);
  }
}

void internal_mark_end(const char* provider) {
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

} // namespace

#ifdef __cplusplus
extern "C" {
#endif

#define PROFILO_EXPORT __attribute__((visibility ("default")))

PROFILO_EXPORT ProfiloApi profilo_api_int {
  .mark_start = &internal_mark_start,
  .mark_end = &internal_mark_end
};

#ifdef __cplusplus
} // extern "C"
#endif
