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

#include <algorithm>

// Needed for MAX_STACK_DEPTH
#include <profiler/Constants.h>

#include <profilo/writer/StackTraceInvertingVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

StackTraceInvertingVisitor::StackTraceInvertingVisitor(EntryVisitor& delegate)
    : delegate_(delegate),
      stack_(std::make_unique<int64_t[]>(MAX_STACK_DEPTH)) {}

void StackTraceInvertingVisitor::visit(const StandardEntry& entry) {
  delegate_.visit(entry);
}

void StackTraceInvertingVisitor::visit(const FramesEntry& entry) {
  if (entry.frames.size > MAX_STACK_DEPTH) {
    throw std::invalid_argument("entry.frames.size > MAX_STACK_DEPTH");
  }

  std::reverse_copy(
      entry.frames.values,
      entry.frames.values + entry.frames.size,
      stack_.get());

  FramesEntry inverted{
      .id = entry.id,
      .type = entry.type,
      .timestamp = entry.timestamp,
      .tid = entry.tid,
      .frames =
          {
              .values = stack_.get(),
              .size = entry.frames.size,
          },
  };
  delegate_.visit(inverted);
}

void StackTraceInvertingVisitor::visit(const BytesEntry& entry) {
  delegate_.visit(entry);
}

} // namespace writer
} // namespace profilo
} // namespace facebook
