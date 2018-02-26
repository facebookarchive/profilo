// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <loom/entries/EntryParser.h>

namespace facebook { namespace profilo { namespace writer {

using namespace entries;

class PrintEntryVisitor: public EntryVisitor {
public:

  PrintEntryVisitor() = delete;
  PrintEntryVisitor(const PrintEntryVisitor&) = delete;

  explicit PrintEntryVisitor(std::ostream& stream);

  virtual void visit(const StandardEntry& data);
  virtual void visit(const FramesEntry& data);
  virtual void visit(const BytesEntry& data);

private:
  std::ostream& stream_;
};

} } } // facebook::profilo::writer
