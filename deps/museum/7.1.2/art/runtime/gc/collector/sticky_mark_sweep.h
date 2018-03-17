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

#ifndef ART_RUNTIME_GC_COLLECTOR_STICKY_MARK_SWEEP_H_
#define ART_RUNTIME_GC_COLLECTOR_STICKY_MARK_SWEEP_H_

#include "base/macros.h"
#include "partial_mark_sweep.h"

namespace art {
namespace gc {
namespace collector {

class StickyMarkSweep FINAL : public PartialMarkSweep {
 public:
  GcType GetGcType() const OVERRIDE {
    return kGcTypeSticky;
  }

  StickyMarkSweep(Heap* heap, bool is_concurrent, const std::string& name_prefix = "");
  ~StickyMarkSweep() {}

  void MarkConcurrentRoots(VisitRootFlags flags)
      OVERRIDE
      REQUIRES(!mark_stack_lock_)
      REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

 protected:
  // Bind the live bits to the mark bits of bitmaps for all spaces, all spaces other than the
  // alloc space will be marked as immune.
  void BindBitmaps() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_);

  void MarkReachableObjects()
      OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void Sweep(bool swap_bitmaps)
      OVERRIDE
      REQUIRES(Locks::heap_bitmap_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StickyMarkSweep);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_STICKY_MARK_SWEEP_H_
