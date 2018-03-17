// Copyright 2004-present Facebook. All Rights Reserved.

#include "logging.h"

namespace art {

namespace {

class NullBuffer : public std::streambuf {
  public:
    int overflow(int c) override { return c; }
};

}

LogMessage::LogMessage(const char* /* unused */, unsigned int /* unused */,
    LogSeverity /* unused */, int /* unused */) {
}

LogMessage::~LogMessage() {
}

std::ostream& LogMessage::stream() {
  static std::ostream null_stream(new NullBuffer);
  return null_stream;
}

void LogMessage::LogLine(const char* /* unused */, unsigned int /* unused */,
    LogSeverity /* unused */, const char* /* unused */) {
}

void LogMessage::LogLineLowStack(const char* /* unused */, unsigned int /* unused */,
    LogSeverity /* unused */, const char* /* unused */) {
}

class LogMessageData {
};

}
