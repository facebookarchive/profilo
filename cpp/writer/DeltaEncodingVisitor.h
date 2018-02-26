// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <loom/entries/EntryParser.h>

namespace facebook {
namespace profilo {
namespace writer {

using namespace entries;

class DeltaEncodingVisitor: public EntryVisitor {
public:

  explicit DeltaEncodingVisitor(EntryVisitor& delegate);

  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

private:
  EntryVisitor& delegate_;

  struct {
    int32_t id;
    int64_t timestamp;
    int32_t tid;
    int32_t callid;
    int32_t matchid;
    int64_t extra;
  } last_values_;
};

} // namespace writer
} // namespace profilo
} // namespace facebook
