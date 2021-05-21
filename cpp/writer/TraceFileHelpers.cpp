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

#include "TraceFileHelpers.h"

#include <errno.h>
#include <profilo/util/common.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <zlib.h>
#include <zstr/zstr.hpp>
#include <sstream>
#include <system_error>

namespace facebook {
namespace profilo {
namespace writer {

namespace {
std::string getTraceIDAsString(int64_t trace_id) {
  const char* kBase64Alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
  const size_t kTraceIdStringLen = 11;

  if (trace_id < 0) {
    throw std::invalid_argument("trace_id must be non-negative");
  }
  char result[kTraceIdStringLen + 1]{};
  for (ssize_t idx = kTraceIdStringLen - 1; idx >= 0; --idx) {
    result[idx] = kBase64Alphabet[trace_id % 64];
    trace_id /= 64;
  }
  return std::string(result);
}

std::string getTraceFilename(
    const std::string& trace_prefix,
    const std::string& trace_id) {
  std::stringstream filename;
  filename << trace_prefix << "-" << getpid() << "-";

  auto now = time(nullptr);
  struct tm localnow {};
  if (localtime_r(&now, &localnow) == nullptr) {
    throw std::runtime_error("Could not localtime_r(3)");
  }

  filename << (1900 + localnow.tm_year) << "-" << (1 + localnow.tm_mon) << "-"
           << localnow.tm_mday << "T" << localnow.tm_hour << "-"
           << localnow.tm_min << "-" << localnow.tm_sec;

  filename << "-" << trace_id << ".tmp";
  return filename.str();
}

std::string sanitize(std::string input) {
  for (size_t idx = 0; idx < input.size(); ++idx) {
    char ch = input[idx];
    bool is_valid = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';

    if (!is_valid) {
      input[idx] = '_';
    }
  }
  return input;
}
} // namespace

void TraceFileHelpers::writeHeaders(
    std::ostream& output,
    int64_t trace_id,
    std::vector<std::pair<std::string, std::string>> const& trace_headers) {
  output << "dt\n"
         << "ver|" << kTraceFormatVersion << "\n"
         << "id|" << getTraceIDAsString(trace_id) << "\n"
         << "prec|" << kTimestampPrecision << "\n";

  for (auto const& header : trace_headers) {
    output << header.first << '|' << header.second << '\n';
  }

  output << '\n';
}

std::string TraceFileHelpers::getTraceFilePath(
    int64_t trace_id,
    std::string const& prefix,
    std::string const& folder) {
  std::stringstream path_stream{};
  const std::string trace_id_string = getTraceIDAsString(trace_id);
  path_stream << folder << '/'
              << sanitize(getTraceFilename(prefix, trace_id_string));
  return path_stream.str();
}

void TraceFileHelpers::ensureFolder(const std::string& folder) {
  struct stat stat_out {};
  if (stat(folder.c_str(), &stat_out)) {
    if (errno != ENOENT) {
      std::string error = std::string("Could not stat() folder ") + folder;
      throw std::system_error(errno, std::system_category(), error);
    }

    // errno == ENOENT, folder needs creating
    mkdirs(folder.c_str());
  }
}

std::unique_ptr<std::ofstream> TraceFileHelpers::openCompressedStream(
    int64_t trace_id,
    std::string const& trace_folder,
    std::string const& trace_prefix) {
  ensureFolder(trace_folder.c_str());

  std::string trace_file =
      TraceFileHelpers::getTraceFilePath(trace_id, trace_prefix, trace_folder);

  auto output = std::make_unique<std::ofstream>(
      trace_file, std::ofstream::out | std::ofstream::binary);
  output->exceptions(std::ofstream::badbit | std::ofstream::failbit);

  zstr::ostreambuf* output_buf = new zstr::ostreambuf(
      output->rdbuf(), // wrap the ofstream buffer
      512 * 1024, // input and output buffers
      3 // compression level
  );

  // Disable ofstream buffering
  output->rdbuf()->pubsetbuf(nullptr, 0);
  // Replace ofstream buffer with the compressed one
  output->basic_ios<char>::rdbuf(output_buf);

  return output;
}

} // namespace writer
} // namespace profilo
} // namespace facebook
