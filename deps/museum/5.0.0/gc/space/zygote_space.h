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

#ifndef ART_RUNTIME_GC_SPACE_ZYGOTE_SPACE_H_
#define ART_RUNTIME_GC_SPACE_ZYGOTE_SPACE_H_

#include "gc/accounting/space_bitmap.h"
#include "malloc_space.h"
#include "mem_map.h"

namespace art {
namespace gc {

namespace space {

// An zygote space is a space which you cannot allocate into or free from.
class ZygoteSpace FINAL : public ContinuousMemMapAllocSpace {
 public:
  // Returns the remaining storage in the out_map field.
  static ZygoteSpace* Create(const std::string& name, MemMap* mem_map,
                             accounting::ContinuousSpaceBitmap* live_bitmap,
                             accounting::ContinuousSpaceBitmap* mark_bitmap)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void Dump(std::ostream& os) const;

  SpaceType GetType() const OVERRIDE {
    return kSpaceTypeZygoteSpace;
  }

  ZygoteSpace* AsZygoteSpace() OVERRIDE {
    return this;
  }

  mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                        size_t* usable_size) OVERRIDE;

  size_t AllocationSize(mirror::Object* obj, size_t* usable_size) OVERRIDE;

  size_t Free(Thread* self, mirror::Object* ptr) OVERRIDE;

  size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) OVERRIDE;

  // ZygoteSpaces don't have thread local state.
  void RevokeThreadLocalBuffers(art::Thread*) OVERRIDE {
  }
  void RevokeAllThreadLocalBuffers() OVERRIDE {
  }

  uint64_t GetBytesAllocated() {
    return Size();
  }

  uint64_t GetObjectsAllocated() {
    return objects_allocated_.LoadSequentiallyConsistent();
  }

  void Clear() OVERRIDE;

  bool CanMoveObjects() const OVERRIDE {
    return false;
  }

  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) OVERRIDE
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 protected:
  virtual accounting::ContinuousSpaceBitmap::SweepCallback* GetSweepCallback() {
    return &SweepCallback;
  }

 private:
  ZygoteSpace(const std::string& name, MemMap* mem_map, size_t objects_allocated);
  static void SweepCallback(size_t num_ptrs, mirror::Object** ptrs, void* arg);

  AtomicInteger objects_allocated_;

  friend class Space;
  DISALLOW_COPY_AND_ASSIGN(ZygoteSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_ZYGOTE_SPACE_H_
