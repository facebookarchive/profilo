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

#include <profilo/entries/EntryParser.h>

namespace facebook {
namespace profilo {
namespace writer {

using namespace entries;

class DeltaEncodingVisitor : public EntryVisitor {
 public:
  explicit DeltaEncodingVisitor(EntryVisitor& delegate);

  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

 private:
  EntryVisitor& delegate_;

  struct {
    int32_t id;
    int64_t timestamp;
    int32_t tid;
    int32_t callid;
    int32_t matchid;
    int64_t extra;
  } last_values_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
