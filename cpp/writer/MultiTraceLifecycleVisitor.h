// Copyright 2004-present Facebook. All Rights Reserved.

#include <unordered_map>
#include <unordered_set>

#include <loom/writer/AbortReason.h>
#include <loom/writer/TraceCallbacks.h>
#include <loom/writer/TraceLifecycleVisitor.h>
#include <loom/RingBuffer.h>

namespace facebook {
namespace loom {
namespace writer {

using namespace facebook::loom::entries;

class MultiTraceLifecycleVisitor: public EntryVisitor {
public:
  MultiTraceLifecycleVisitor(
    const std::string& folder,
    const std::string& trace_prefix,
    std::shared_ptr<TraceCallbacks> callbacks,
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::function<void(TraceLifecycleVisitor& visitor)> trace_backward_callback);
  virtual void visit(const StandardEntry& entry) override;
  virtual void visit(const FramesEntry& entry) override;
  virtual void visit(const BytesEntry& entry) override;

  void abort(AbortReason reason);
  bool done();
  std::unordered_set<int64_t> getConsumedTraces();

private:
  const std::string& folder_;
  const std::string& trace_prefix_;
  std::shared_ptr<TraceCallbacks> callbacks_;
  const std::vector<std::pair<std::string, std::string>> trace_headers_;

  std::unordered_map<int64_t, TraceLifecycleVisitor> visitors_;
  std::unordered_set<int64_t> consumed_traces_;
  std::function<void(TraceLifecycleVisitor& visitor)> trace_backward_callback_;

  bool done_;
};

} // namespace writer
} // namespace loom
} // namespace facebook
