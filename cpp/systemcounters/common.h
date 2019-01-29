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

#include <profilo/LogEntry.h>
#include <profilo/entries/EntryType.h>

using facebook::profilo::entries::COUNTER;
using facebook::profilo::entries::StandardEntry;

namespace facebook {
namespace profilo {

template <typename Logger>
__attribute__((always_inline)) static inline void logCounter(
    Logger& logger,
    int32_t counter_name,
    int64_t value,
    int32_t thread_id,
    int64_t time) {
  logger.write(StandardEntry{
      .id = 0,
      .type = COUNTER,
      .timestamp = time,
      .tid = thread_id,
      .callid = counter_name,
      .matchid = 0,
      .extra = value,
  });
}

template <typename Logger>
__attribute__((always_inline)) static inline void logNonMonotonicCounter(
    int64_t prev_value,
    int64_t value,
    int32_t thread_id,
    int64_t time,
    int32_t quicklog_id,
    Logger& logger = Logger::get()) {
  if (prev_value == value) {
    return;
  }
  logCounter(logger, quicklog_id, value, thread_id, time);
}

template <typename Logger>
__attribute__((always_inline)) static inline void logMonotonicCounter(
    uint64_t prev,
    uint64_t curr,
    int tid,
    int64_t time,
    uint32_t quicklog_id,
    Logger& logger = Logger::get(),
    int64_t prev_skipped_time = 0,
    int threshold = 0) {
  if (curr > prev + threshold) {
    logCounter(logger, quicklog_id, curr, tid, time);
    if (prev_skipped_time) {
      // If it's non-zero it means that the previous
      // sample didn't result in a log point.
      logCounter(logger, quicklog_id, prev, tid, prev_skipped_time);
    }
  }
}

} // namespace profilo
} // namespace facebook
