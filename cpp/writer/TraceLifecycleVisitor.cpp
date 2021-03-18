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

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <util/common.h>
#include <zlib.h>
#include <sstream>
#include <system_error>

#include <profilo/writer/DeltaEncodingVisitor.h>
#include <profilo/writer/PrintEntryVisitor.h>
#include <profilo/writer/StackTraceInvertingVisitor.h>
#include <profilo/writer/TimestampTruncatingVisitor.h>
#include <profilo/writer/TraceLifecycleVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

using namespace facebook::profilo::entries;

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

void ensureFolder(const char* folder) {
  struct stat stat_out {};
  if (stat(folder, &stat_out)) {
    if (errno != ENOENT) {
      std::string error = std::string("Could not stat() folder ") + folder;
      throw std::system_error(errno, std::system_category(), error);
    }

    // errno == ENOENT, folder needs creating
    mkdirs(folder);
  }
}

} // namespace

TraceLifecycleVisitor::TraceLifecycleVisitor(
    const std::string& trace_folder,
    const std::string& trace_prefix,
    std::shared_ptr<TraceCallbacks> callbacks,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int64_t trace_id,
    std::function<void(TraceLifecycleVisitor& visitor)> trace_backward_callback)
    :

      trace_folder_(trace_folder),
      trace_prefix_(trace_prefix),
      trace_headers_(headers),
      output_(nullptr),
      delegates_(),
      expected_trace_(trace_id),
      callbacks_(callbacks),
      started_(false),
      done_(false),
      trace_backward_callback_(std::move(trace_backward_callback)) {}

void TraceLifecycleVisitor::visit(const StandardEntry& entry) {
  auto type = static_cast<EntryType>(entry.type);
  switch (type) {
    case EntryType::TRACE_END: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceEnd(trace_id);
      break;
    }
    case EntryType::TRACE_TIMEOUT:
    case EntryType::TRACE_ABORT: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      auto reason = type == EntryType::TRACE_TIMEOUT
          ? AbortReason::TIMEOUT
          : AbortReason::CONTROLLER_INITIATED;

      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceAbort(trace_id, reason);
      break;
    }
    case EntryType::TRACE_BACKWARDS:
    case EntryType::TRACE_START: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      onTraceStart(trace_id, entry.matchid);
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }

      if (type == EntryType::TRACE_BACKWARDS) {
        trace_backward_callback_(*this);
      }
      break;
    }
    case EntryType::LOGGER_PRIORITY: {
      if (expected_trace_ == entry.extra) {
        thread_priority_ = std::make_unique<ScopedThreadPriority>(entry.callid);
      }
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      break;
    }
    default: {
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
    }
  }
}

void TraceLifecycleVisitor::visit(const FramesEntry& entry) {
  if (hasDelegate()) {
    delegates_.back()->visit(entry);
  }
}

void TraceLifecycleVisitor::visit(const BytesEntry& entry) {
  if (hasDelegate()) {
    delegates_.back()->visit(entry);
  }
}

void TraceLifecycleVisitor::abort(AbortReason reason) {
  onTraceAbort(expected_trace_, reason);
}

void TraceLifecycleVisitor::onTraceStart(int64_t trace_id, int32_t flags) {
  if (output_ != nullptr) {
    // active trace with same ID, abort
    abort(AbortReason::NEW_START);
    return;
  }

  ensureFolder(trace_folder_.c_str());

  std::stringstream path_stream;
  const std::string trace_id_string = getTraceIDAsString(trace_id);
  path_stream << trace_folder_ << '/'
              << sanitize(getTraceFilename(trace_prefix_, trace_id_string));

  std::string trace_file = path_stream.str();

  output_ = std::make_unique<std::ofstream>(
      trace_file, std::ofstream::out | std::ofstream::binary);
  output_->exceptions(std::ofstream::badbit | std::ofstream::failbit);

  zstr::ostreambuf* output_buf_ = new zstr::ostreambuf(
      output_->rdbuf(), // wrap the ofstream buffer
      512 * 1024, // input and output buffers
      3 // compression level
  );

  // Disable ofstream buffering
  output_->rdbuf()->pubsetbuf(nullptr, 0);
  // Replace ofstream buffer with the compressed one
  output_->basic_ios<char>::rdbuf(output_buf_);

  writeHeaders(*output_, trace_id_string);

  // outputTime = truncate(current) - truncate(prev)
  delegates_.emplace_back(new PrintEntryVisitor(*output_));
  delegates_.emplace_back(new DeltaEncodingVisitor(*delegates_.back()));
  delegates_.emplace_back(
      new TimestampTruncatingVisitor(*delegates_.back(), kTimestampPrecision));
  delegates_.emplace_back(new StackTraceInvertingVisitor(*delegates_.back()));

  if (callbacks_.get() != nullptr) {
    callbacks_->onTraceStart(trace_id, flags, trace_file);
  }

  started_ = true;
  done_ = false;
}

void TraceLifecycleVisitor::onTraceAbort(int64_t trace_id, AbortReason reason) {
  done_ = true;
  cleanupState();
  if (started_ && callbacks_.get() != nullptr) {
    callbacks_->onTraceAbort(trace_id, reason);
  }
}

void TraceLifecycleVisitor::onTraceEnd(int64_t trace_id) {
  done_ = true;
  cleanupState();
  if (started_ && callbacks_.get() != nullptr) {
    callbacks_->onTraceEnd(trace_id);
  }
}

void TraceLifecycleVisitor::cleanupState() {
  delegates_.clear();
  thread_priority_ = nullptr;
  if (output_) {
    output_->flush();
    output_->close();
    output_ = nullptr;
  }
}

void TraceLifecycleVisitor::writeHeaders(std::ostream& output, std::string id) {
  output << "dt\n"
         << "ver|" << kTraceFormatVersion << "\n"
         << "id|" << id << "\n"
         << "prec|" << kTimestampPrecision << "\n";

  for (auto const& header : trace_headers_) {
    output << header.first << '|' << header.second << '\n';
  }

  output << '\n';
}

} // namespace writer
} // namespace profilo
} // namespace facebook
