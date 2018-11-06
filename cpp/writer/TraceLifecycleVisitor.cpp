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

const int kTraceOutputBufSize = 1 << 19; // 512K

/**
 * Buffer which extends zstr:ostreambuf to intercept buffer overflows and
 * calculate crc32 checksum.
 */
class crc_ofstreambuf : public zstr::ostreambuf {
 public:
  explicit crc_ofstreambuf(std::streambuf* _sbuf_p, uint32_t& crc)
      : zstr::ostreambuf(_sbuf_p, kTraceOutputBufSize, Z_DEFAULT_COMPRESSION),
        crc_(crc) {
    crc_ = crc32(0, Z_NULL, 0);
  }

  virtual ~crc_ofstreambuf() {
    sync();
  }

 protected:
  virtual std::streambuf::int_type overflow(
      std::streambuf::int_type ch = traits_type::eof()) {
    crc_ = crc32(
        crc_,
        reinterpret_cast<const unsigned char*>(pbase()),
        pptr() - pbase());
    return zstr::ostreambuf::overflow(ch);
  }

  uint32_t& crc_;
};

/**
 * File output stream with custom buffer implementation crc_ofstreambuf which
 * computes crc32 checksum of all the data written to it.
 */
class crc_ofstream
    : private zstr::detail::strict_fstream_holder<strict_fstream::ofstream>,
      public std::ostream {
 public:
  explicit crc_ofstream(
      const std::string& filename,
      uint32_t& crc,
      std::ios_base::openmode mode = std::ios_base::out)
      : zstr::detail::strict_fstream_holder<strict_fstream::ofstream>(
            filename,
            mode | std::ios_base::binary),
        std::ostream(new crc_ofstreambuf(_fs.rdbuf(), crc)) {
    exceptions(std::ios_base::badbit);
  }

  virtual ~crc_ofstream() {
    if (!rdbuf())
      return;
    delete rdbuf();
  }
};

std::string getTraceID(int64_t trace_id) {
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
    const std::string& folder,
    const std::string& trace_prefix,
    std::shared_ptr<TraceCallbacks> callbacks,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int64_t trace_id)
    :

      folder_(folder),
      trace_prefix_(trace_prefix),
      trace_headers_(headers),
      output_(nullptr),
      crc_(0),
      delegates_(),
      expected_trace_(trace_id),
      callbacks_(callbacks),
      done_(false) {}

void TraceLifecycleVisitor::visit(const StandardEntry& entry) {
  switch (entry.type) {
    case entries::TRACE_END: {
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
    case entries::TRACE_TIMEOUT:
    case entries::TRACE_ABORT: {
      int64_t trace_id = entry.extra;
      if (trace_id != expected_trace_) {
        return;
      }
      auto reason = entry.type == entries::TRACE_TIMEOUT
          ? AbortReason::TIMEOUT
          : AbortReason::CONTROLLER_INITIATED;

      // write before we clean up state
      if (hasDelegate()) {
        delegates_.back()->visit(entry);
      }
      onTraceAbort(trace_id, reason);
      break;
    }
    case entries::TRACE_BACKWARDS:
    case entries::TRACE_START: {
      onTraceStart(entry.extra, entry.matchid);
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
  if (trace_id != expected_trace_) {
    return;
  }

  if (output_ != nullptr) {
    // active trace with same ID, abort
    abort(AbortReason::NEW_START);
    return;
  }

  std::stringstream path_stream;
  std::string trace_id_string = getTraceID(trace_id);
  path_stream << folder_ << '/' << sanitize(trace_id_string);

  //
  // Note that the construction of this path must match the computation in
  // TraceOrchestrator.getSanitizedTraceFolder. Unfortunately, it's far too
  // gnarly to share this code at the moment.
  //
  std::string trace_folder = path_stream.str();
  try {
    ensureFolder(trace_folder.c_str());
  } catch (const std::system_error& ex) {
    // Add more diagnostics to the exception.
    // Namely, parent folder owner uid and gid, as
    // well as our own uid and gid.
    struct stat stat_out {};

    std::stringstream error;
    if (stat(folder_.c_str(), &stat_out)) {
      error << "Could not stat(" << folder_
            << ").\nOriginal exception: " << ex.what();
      throw std::system_error(errno, std::system_category(), error.str());
    }

    error << "Could not create trace folder " << trace_folder
          << ".\nOriginal exception: " << ex.what() << ".\nDebug info for "
          << folder_ << ": uid=" << stat_out.st_uid
          << "; gid=" << stat_out.st_gid << "; proc euid=" << geteuid()
          << "; proc egid=" << getegid();
    throw std::system_error(ex.code(), error.str());
  }

  path_stream << '/'
              << sanitize(getTraceFilename(trace_prefix_, trace_id_string));

  std::string trace_file = path_stream.str();

  output_ = std::unique_ptr<std::ostream>(new crc_ofstream(trace_file, crc_));

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

  done_ = false;
}

void TraceLifecycleVisitor::onTraceAbort(int64_t trace_id, AbortReason reason) {
  done_ = true;
  cleanupState();
  if (callbacks_.get() != nullptr) {
    callbacks_->onTraceAbort(trace_id, reason);
  }
}

void TraceLifecycleVisitor::onTraceEnd(int64_t trace_id) {
  done_ = true;
  cleanupState();
  if (callbacks_.get() != nullptr) {
    callbacks_->onTraceEnd(trace_id, crc_);
  }
}

void TraceLifecycleVisitor::cleanupState() {
  delegates_.clear();
  output_ = nullptr;
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
