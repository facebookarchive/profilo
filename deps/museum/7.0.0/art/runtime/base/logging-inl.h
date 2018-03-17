// Copyright 2004-present Facebook. All Rights Reserved.

#include <museum/7.0.0/art/runtime/base/logging.h>

#include <museum/7.0.0/external/libcxx/sstream>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

/* copied directly from AOSP base/logging.cc */
/* won't actually be used, just need correct sizing information */
class LogMessageData {
 private:
  std::ostringstream buffer_;
  const char* const file_;
  const unsigned int line_number_;
  const LogSeverity severity_;
  const int error_;

  DISALLOW_COPY_AND_ASSIGN(LogMessageData);
};

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
