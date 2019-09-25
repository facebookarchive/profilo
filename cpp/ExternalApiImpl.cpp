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

#include <profilo/ExternalApiGlue.h>

#include <unistd.h>
#include <string>

#include <profiler/ExternalTracerManager.h>
#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <profilo/TraceProviders.h>

using namespace facebook::profilo;

namespace {

std::string& providerToName(Provider provider) {
  switch (provider) {
    case Provider::CLASS_LOAD:
      static std::string class_load = "class_load";
      return class_load;
    case Provider::FBSYSTRACE:
      static std::string fbsystrace = "fbsystrace";
      return fbsystrace;
    default:
      abort();
  }
}

void internal_mark_start(Provider provider, const char* msg, size_t len = 0) {
  if (msg == nullptr ||
      !TraceProviders::get().isEnabled(providerToName(provider))) {
    return;
  }
  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::MARK_PUSH;
  int32_t id = logger.write(std::move(entry));
  size_t msg_len = len == 0 ? strlen(msg) : len;

  if (msg_len > 0) {
    logger.writeBytes(entries::STRING_NAME, id, (const uint8_t*)msg, msg_len);
  }
}

void internal_mark_end(Provider provider) {
  if (!TraceProviders::get().isEnabled(providerToName(provider))) {
    return;
  }
  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::MARK_POP;
  logger.write(std::move(entry));
}

void internal_log_classload_start(Provider provider) {
  if (!TraceProviders::get().isEnabled(providerToName(provider))) {
    return;
  }

  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::CLASS_LOAD_START;
  entry.extra = 0;
  logger.write(std::move(entry));
}

void internal_log_classload_end(Provider provider, int64_t classid) {
  if (!TraceProviders::get().isEnabled(providerToName(provider))) {
    return;
  }

  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::CLASS_LOAD_END;
  entry.extra = classid;
  logger.write(std::move(entry));
}

void internal_log_classload_failed(Provider provider) {
  if (!TraceProviders::get().isEnabled(providerToName(provider))) {
    return;
  }

  auto& logger = Logger::get();
  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = entries::CLASS_LOAD_FAILED;
  entry.extra = 0;
  logger.write(std::move(entry));
}

bool is_enabled(Provider provider) {
  return TraceProviders::get().isEnabled(providerToName(provider));
}

bool internal_register_external_tracer_callback(
    int tracerType,
    profilo_int_collect_stack_fn callback) {
  return profiler::ExternalTracerManager::getInstance().registerCallback(
      tracerType, callback);
}

} // namespace

// Activates the External API by setting actual struct implementation.
__attribute__((constructor)) void init_profilo_external_api() {
  profilo_api_int.mark_start = &internal_mark_start;
  profilo_api_int.mark_end = &internal_mark_end;
  profilo_api_int.log_classload_start = &internal_log_classload_start;
  profilo_api_int.log_classload_end = &internal_log_classload_end;
  profilo_api_int.log_classload_failed = &internal_log_classload_failed;
  profilo_api_int.is_enabled = &is_enabled;
  profilo_api_int.register_external_tracer_callback =
      &internal_register_external_tracer_callback;
}
