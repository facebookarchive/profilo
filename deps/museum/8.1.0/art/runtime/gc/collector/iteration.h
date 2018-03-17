/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_ITERATION_H_
#define ART_RUNTIME_GC_COLLECTOR_ITERATION_H_

#include <inttypes.h>
#include <vector>

#include "android-base/macros.h"
#include "base/timing_logger.h"
#include "object_byte_pair.h"

namespace art {
namespace gc {
namespace collector {

// A information related single garbage collector iteration. Since we only ever have one GC running
// at any given time, we can have a single iteration info.
class Iteration {
 public:
  Iteration();
  // Returns how long the mutators were paused in nanoseconds.
  const std::vector<uint64_t>& GetPauseTimes() const {
    return pause_times_;
  }
  TimingLogger* GetTimings() {
    return &timings_;
  }
  // Returns how long the GC took to complete in nanoseconds.
  uint64_t GetDurationNs() const {
    return duration_ns_;
  }
  int64_t GetFreedBytes() const {
    return freed_.bytes;
  }
  int64_t GetFreedLargeObjectBytes() const {
    return freed_los_.bytes;
  }
  uint64_t GetFreedObjects() const {
    return freed_.objects;
  }
  uint64_t GetFreedLargeObjects() const {
    return freed_los_.objects;
  }
  uint64_t GetFreedRevokeBytes() const {
    return freed_bytes_revoke_;
  }
  void SetFreedRevoke(uint64_t freed) {
    freed_bytes_revoke_ = freed;
  }
  void Reset(GcCause gc_cause, bool clear_soft_references);
  // Returns the estimated throughput of the iteration.
  uint64_t GetEstimatedThroughput() const;
  bool GetClearSoftReferences() const {
    return clear_soft_references_;
  }
  void SetClearSoftReferences(bool clear_soft_references) {
    clear_soft_references_ = clear_soft_references;
  }
  GcCause GetGcCause() const {
    return gc_cause_;
  }

 private:
  void SetDurationNs(uint64_t duration) {
    duration_ns_ = duration;
  }

  GcCause gc_cause_;
  bool clear_soft_references_;
  uint64_t duration_ns_;
  TimingLogger timings_;
  ObjectBytePair freed_;
  ObjectBytePair freed_los_;
  uint64_t freed_bytes_revoke_;  // see Heap::num_bytes_freed_revoke_.
  std::vector<uint64_t> pause_times_;

  friend class GarbageCollector;
  DISALLOW_COPY_AND_ASSIGN(Iteration);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_ITERATION_H_
