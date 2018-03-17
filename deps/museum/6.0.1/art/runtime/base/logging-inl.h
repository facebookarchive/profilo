/**
 * Copyright 2018-present, Facebook, Inc.
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

#include <museum/6.0.1/art/runtime/base/logging.h>

#include <museum/6.0.1/external/libcxx/sstream>

namespace art {

/* copied directly from AOSP base/logging.cc */
/* won't actually be used, just need correct sizing information */
class LogMessageData {
 public:
  LogMessageData(const char* file, unsigned int line, LogSeverity severity, int error)
      : file_(file),
        line_number_(line),
        severity_(severity),
        error_(error) {
    const char* last_slash = strrchr(file, '/');
    file = (last_slash == nullptr) ? file : last_slash + 1;
  }

  const char * GetFile() const {
    return file_;
  }

  unsigned int GetLineNumber() const {
    return line_number_;
  }

  LogSeverity GetSeverity() const {
    return severity_;
  }

  int GetError() const {
    return error_;
  }

  std::ostream& GetBuffer() {
    return buffer_;
  }

  std::string ToString() const {
    return buffer_.str();
  }

 private:
  std::ostringstream buffer_;
  const char* const file_;
  const unsigned int line_number_;
  const LogSeverity severity_;
  const int error_;

  DISALLOW_COPY_AND_ASSIGN(LogMessageData);
};

}
