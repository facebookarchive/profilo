// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <loom/entries/EntryParser.h>

namespace facebook {
namespace profilo {
namespace writer {

using namespace entries;

class TimestampTruncatingVisitor: public EntryVisitor {
public:

  //
  // precision: orders of magnitude of precision.
  //            E.g., 6 == 10e-6 == microseconds
  //
  explicit TimestampTruncatingVisitor(
    EntryVisitor& delegate,
    size_t precision = 6);

  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

private:
  EntryVisitor& delegate_;
  int64_t denominator_;

  template<class T>
  T truncateTimestamp(const T& entry);
};

} // namespace writer
} // namespace profilo
} // namespace facebook
