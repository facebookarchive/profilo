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
#include <profilo/MultiBufferLogger.h>
#include <sys/types.h>
#include <stdexcept>

using facebook::profilo::logger::MultiBufferLogger;

namespace facebook {
namespace profilo {
namespace counters {

struct CounterPoint {
  int64_t value = INT64_MIN;
  int64_t timestamp;
  bool writeSkipped;
};

//
// Provides API for individual counter tracking and logging.
//
// There are 2 main rules all the counters must obey:
// 1) It ensures that a counter which doesn't change it's value between samples
// is not logged twice. I.e. if c_i = a at t_i and c_i+1 = a at t_i+1, then
// logging is done only for c_i at t_i
//
// * ----- x
//
// [*] - logged point
// [x] - skipped point
//
// 2) If a value changes at the next sample and a
// previous point was ignored because of previous rule then that previous point
// is logged to mark the actual value change explicitly. I.e. if c_i = a, c_i+1
// = a, c_i+2 = a, c_i+3 = b recorded at corresponding times, then c_i, c_i+2,
// c_i+3 will be logged.
//
//                  *
//                /
//  * --- x --- *
//
// Every call to recordAndLog() method moves the counter state and logs points
// if necessary.
//
class Counter {
 public:
  Counter(MultiBufferLogger& logger, int32_t counterType, int32_t tid)
      : counterType_(counterType), point_(), logger_(logger), tid_(tid) {}

  // Records and logs if necessary next counter value.
  // Timestamp must always increase.
  void record(int64_t value, int64_t timestamp) {
    if (timestamp <= point_.timestamp) {
      throw std::runtime_error("Timestamp must always increase");
    }
    if (point_.value == value) {
      point_.timestamp = timestamp;
      point_.writeSkipped = true;
      return;
    }
    if (point_.writeSkipped) {
      log();
    }
    point_ = CounterPoint{
        .value = value,
        .timestamp = timestamp,
    };
    log();
  }

 private:
  void log() {
    logger_.write(StandardEntry{
        .id = 0,
        .type = EntryType::COUNTER,
        .timestamp = point_.timestamp,
        .tid = tid_,
        .callid = counterType_,
        .matchid = 0,
        .extra = point_.value,
    });
  }

  int32_t counterType_;
  CounterPoint point_;
  MultiBufferLogger& logger_;
  int32_t tid_;
};

using TraceCounter = Counter;

} // namespace counters
} // namespace profilo
} // namespace facebook
