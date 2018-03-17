/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
#define ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_

#include "garbage_collector.h"

namespace art {
namespace gc {
namespace collector {

class ConcurrentCopying : public GarbageCollector {
 public:
  explicit ConcurrentCopying(Heap* heap, bool generational = false,
                             const std::string& name_prefix = "")
      : GarbageCollector(heap,
                         name_prefix + (name_prefix.empty() ? "" : " ") +
                         "concurrent copying + mark sweep") {}

  ~ConcurrentCopying() {}

  virtual void RunPhases() OVERRIDE {}
  virtual GcType GetGcType() const OVERRIDE {
    return kGcTypePartial;
  }
  virtual CollectorType GetCollectorType() const OVERRIDE {
    return kCollectorTypeCC;
  }
  virtual void RevokeAllThreadLocalBuffers() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConcurrentCopying);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
