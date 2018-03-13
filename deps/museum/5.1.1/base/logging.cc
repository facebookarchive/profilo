#include "logging.h"

#include <string.h>

namespace art {

/* copied directly from AOSP base/logging.cc */
LogMessageData::LogMessageData(const char* file, int line, LogSeverity severity, int error)
    : file(file),
      line_number(line),
      severity(severity),
      error(error) {
  const char* last_slash = strrchr(file, '/');
  file = (last_slash == NULL) ? file : last_slash + 1;
}

} // namespace art
