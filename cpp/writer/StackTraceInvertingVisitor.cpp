// Copyright 2004-present Facebook. All Rights Reserved.

#include <algorithm>

// Needed for MAX_STACK_DEPTH
#include <profiler/Constants.h>

#include <loom/writer/StackTraceInvertingVisitor.h>

namespace facebook {
namespace profilo {
namespace writer {

StackTraceInvertingVisitor::StackTraceInvertingVisitor(
  EntryVisitor& delegate
):
  delegate_(delegate),
  stack_(std::make_unique<int64_t[]>(MAX_STACK_DEPTH))
{}

void StackTraceInvertingVisitor::visit(const StandardEntry& entry) {
  delegate_.visit(entry);
}

void StackTraceInvertingVisitor::visit(const FramesEntry& entry) {
  if (entry.frames.size > MAX_STACK_DEPTH) {
    throw std::invalid_argument("entry.frames.size > MAX_STACK_DEPTH");
  }

  std::reverse_copy(
    entry.frames.values,
    entry.frames.values + entry.frames.size,
    stack_.get()
  );

  FramesEntry inverted {
    .id = entry.id,
    .type = entry.type,
    .timestamp = entry.timestamp,
    .tid = entry.tid,
    .frames = {
      .values = stack_.get(),
      .size = entry.frames.size,
    },
  };
  delegate_.visit(inverted);
}

void StackTraceInvertingVisitor::visit(const BytesEntry& entry) {
  delegate_.visit(entry);
}

} // writer
} // profilo
} // facebook
