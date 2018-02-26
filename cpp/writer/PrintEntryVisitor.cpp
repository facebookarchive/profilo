// Copyright 2004-present Facebook. All Rights Reserved.

#include <loom/writer/PrintEntryVisitor.h>

namespace facebook { namespace profilo { namespace writer {

PrintEntryVisitor::PrintEntryVisitor(std::ostream& stream):
  stream_(stream) {}

void PrintEntryVisitor::visit(const StandardEntry& data) {
  stream_ << data.id;
  stream_ << '|';
  stream_ << entries::to_string((EntryType) data.type);
  stream_ << '|';
  stream_ << data.timestamp;
  stream_ << '|';
  stream_ << data.tid;
  stream_ << '|';
  stream_ << data.callid;
  stream_ << '|';
  stream_ << data.matchid;
  stream_ << '|';
  stream_ << data.extra;
  stream_ << '\n';
}

void PrintEntryVisitor::visit(const FramesEntry& data) {
  for (size_t idx = 0; idx < data.frames.size; ++idx) {
    stream_ << data.id;
    stream_ << '|';
    stream_ << entries::to_string((EntryType) data.type);
    stream_ << '|';
    stream_ << data.timestamp;
    stream_ << '|';
    stream_ << data.tid;
    stream_ << "|0|0|";
    stream_ << data.frames.values[idx];
    stream_ << '\n';
  }

}

void PrintEntryVisitor::visit(const BytesEntry& data) {
  stream_ << data.id;
  stream_ << '|';
  stream_ << entries::to_string((EntryType) data.type);
  stream_ << '|';
  stream_ << data.matchid;
  stream_ << '|';
  {
    char _bytes_buf[data.bytes.size + 1];
    memset(_bytes_buf, 0, data.bytes.size + 1);
    memcpy(_bytes_buf, data.bytes.values, data.bytes.size);
    stream_ << _bytes_buf;
  }
  stream_ << '\n';
}

} } } // facebook::profilo::writer
