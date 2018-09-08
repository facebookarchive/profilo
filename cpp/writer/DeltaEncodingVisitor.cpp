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

#include <profilo/writer/DeltaEncodingVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

DeltaEncodingVisitor::DeltaEncodingVisitor(EntryVisitor& delegate)
    : delegate_(delegate), last_values_() {}

void DeltaEncodingVisitor::visit(const StandardEntry& entry) {
  StandardEntry encoded{
      .id = entry.id - last_values_.id,
      .type = entry.type,
      .timestamp = entry.timestamp - last_values_.timestamp,
      .tid = entry.tid - last_values_.tid,
      .callid = entry.callid - last_values_.callid,
      .matchid = entry.matchid - last_values_.matchid,
      .extra = entry.extra - last_values_.extra,
  };

  last_values_ = {
      .id = entry.id,
      .timestamp = entry.timestamp,
      .tid = entry.tid,
      .callid = entry.callid,
      .matchid = entry.matchid,
      .extra = entry.extra,
  };

  delegate_.visit(encoded);
}

void DeltaEncodingVisitor::visit(const FramesEntry& entry) {
  for (int32_t idx = 0; idx < entry.frames.size; ++idx) {
    int64_t current_frame = entry.frames.values[idx];

    int64_t frames[1] = {current_frame - last_values_.extra};

    FramesEntry encoded{.id = entry.id - last_values_.id + idx,
                        .type = entry.type,
                        .timestamp = entry.timestamp - last_values_.timestamp,
                        .tid = entry.tid - last_values_.tid,
                        .frames = {.values = frames, .size = 1}};

    last_values_ = {
        .id = entry.id + idx,
        .timestamp = entry.timestamp,
        .tid = entry.tid,

        // FramesEntries don't use callid and matchid, it's okay
        // to preserve them to whatever they were before this entry.
        .callid = last_values_.callid,
        .matchid = last_values_.matchid,

        .extra = current_frame,
    };
    delegate_.visit(encoded);
  }
}

void DeltaEncodingVisitor::visit(const BytesEntry& entry) {
  // BytesEntry is not delta-encoded
  delegate_.visit(entry);
}

} // namespace writer
} // namespace profilo
} // namespace facebook
