#include "logging.h"

#include <sstream>

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
