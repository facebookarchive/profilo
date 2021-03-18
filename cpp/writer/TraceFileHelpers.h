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

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace facebook {
namespace profilo {
namespace writer {

class TraceFileHelpers {
 public:
  // Timestamp precision is microsec by default.
  static constexpr size_t kTimestampPrecision = 6;
  static constexpr size_t kTraceFormatVersion = 3;

  static void writeHeaders(
      std::ostream& output,
      int64_t id,
      std::vector<std::pair<std::string, std::string>> const& trace_headers);
  static std::unique_ptr<std::ofstream> openCompressedStream(
      int64_t trace_id,
      std::string const& trace_folder,
      std::string const& trace_prefix);

 private:
  static std::string getTraceFilePath(
      int64_t trace_id,
      std::string const& prefix,
      std::string const& folder);
  static void ensureFolder(std::string const& folder);
};

} // namespace writer
} // namespace profilo
} // namespace facebook
