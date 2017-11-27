// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <loom/entries/EntryParser.h>

namespace facebook {
namespace loom {
namespace writer {

using namespace entries;

/**
 * A visitor whose only purpose is to reverse the order
 * of frames in a FramesEntry.
 * The profiler gives us frames in bottom-first format (since that's natural
 * for an unwinder). The file format however expects top-first.
 */
class StackTraceInvertingVisitor: public EntryVisitor {
public:

  explicit StackTraceInvertingVisitor(EntryVisitor& delegate);

  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

private:
  EntryVisitor& delegate_;
  std::unique_ptr<int64_t[]> stack_;
};

} // namespace writer
} // namespace loom
} // namespace facebook
