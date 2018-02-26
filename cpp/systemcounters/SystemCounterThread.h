// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <fbjni/fbjni.h>
#include <util/ProcFs.h>

#include <mutex>
#include <unordered_map>

namespace facebook {
namespace profilo {

class SystemCounterThread : public facebook::jni::HybridClass<SystemCounterThread> {
 public:
  constexpr static auto kJavaDescriptor = "Lcom/facebook/loom/provider/systemcounters/SystemCounterThread;";

  static facebook::jni::local_ref<jhybriddata> initHybrid(facebook::jni::alias_ref<jclass>);

  static void registerNatives();

  void logCounters();
  void logThreadCounters(int tid);
  void logTraceAnnotations(int tid);

 private:
  friend HybridBase;
  SystemCounterThread() = default;

  std::mutex mtx_; // Guards cache_
  util::ThreadCache cache_;
  std::unique_ptr<util::TaskStatFile> processStatFile_;

  void logProcessCounters();
};

} // namespace profilo
} // namespace facebook
