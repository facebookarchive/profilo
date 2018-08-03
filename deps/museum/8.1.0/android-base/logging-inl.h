// Copyright 2004-present Facebook. All Rights Reserved.

#include <museum/8.1.0/android-base/logging.h>

#include <museum/8.1.0/external/libcxx/sstream>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace android { namespace base {

/* copied directly AOSP base/logging.cc */
/* won't actually be used, just need correct sizing information */
class LogMessageData {
 public:
  std::ostringstream buffer_;
  const char* const file_;
  const unsigned int line_number_;
  const LogId id_;
  const LogSeverity severity_;
  const int error_;

  DISALLOW_COPY_AND_ASSIGN(LogMessageData);
};

} } } } } // namespace facebook::museum::MUSEUM_VERSION::android::base
