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

#include <cstring>

#include <profilo/writer/TimestampTruncatingVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

namespace {

int64_t calculateDenominator(size_t precision) {
  if (precision > 9) {
    throw std::out_of_range("precision must be between 0 and 9 inclusive");
  }
  const size_t kPrecisionTimestamp = 9; // timestamps are in nanoseconds
  const size_t ordersDifference = kPrecisionTimestamp - precision;

  int64_t denom = 1;
  for (size_t idx = 0; idx < ordersDifference; idx++) {
    denom *= 10;
  }
  return denom;
}


} // anonymous

TimestampTruncatingVisitor::TimestampTruncatingVisitor(
  EntryVisitor& delegate,
  size_t precision
):
  delegate_(delegate),
  denominator_(calculateDenominator(precision))
{}

template<class T>
T TimestampTruncatingVisitor::truncateTimestamp(const T& entry) {
  T copied(entry);
  auto newts = (copied.timestamp + denominator_ / 2) / denominator_;
  copied.timestamp = newts;
  return copied;
}

void TimestampTruncatingVisitor::visit(const StandardEntry& entry) {
  auto std_entry = entry;
  delegate_.visit(truncateTimestamp(std_entry));
}

void TimestampTruncatingVisitor::visit(const FramesEntry& entry) {
  auto frames_entry = entry;
  delegate_.visit(truncateTimestamp(frames_entry));
}

void TimestampTruncatingVisitor::visit(const BytesEntry& entry) {
  delegate_.visit(entry);
}

} // writer
} // profilo
} // facebook
