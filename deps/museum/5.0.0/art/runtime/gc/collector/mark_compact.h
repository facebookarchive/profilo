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

#ifndef ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_H_
#define ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_H_

#include <deque>
#include <memory>  // For unique_ptr.

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "garbage_collector.h"
#include "gc/accounting/heap_bitmap.h"
#include "immune_region.h"
#include "lock_word.h"
#include "object_callbacks.h"
#include "offsets.h"

namespace art {

class Thread;

namespace mirror {
  class Class;
  class Object;
}  // namespace mirror

namespace gc {

class Heap;

namespace accounting {
  template <typename T> class AtomicStack;
  typedef AtomicStack<mirror::Object*> ObjectStack;
}  // namespace accounting

namespace space {
  class BumpPointerSpace;
  class ContinuousMemMapAllocSpace;
  class ContinuousSpace;
}  // namespace space

namespace collector {

class MarkCompact : public GarbageCollector {
 public:
  explicit MarkCompact(Heap* heap, const std::string& name_prefix = "");
  ~MarkCompact() {}

  virtual void RunPhases() OVERRIDE NO_THREAD_SAFETY_ANALYSIS;
  void InitializePhase();
  void MarkingPhase() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::heap_bitmap_lock_);
  void ReclaimPhase() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::heap_bitmap_lock_);
  void FinishPhase() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  void MarkReachableObjects()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  virtual GcType GetGcType() const OVERRIDE {
    return kGcTypePartial;
  }
  virtual CollectorType GetCollectorType() const OVERRIDE {
    return kCollectorTypeMC;
  }

  // Sets which space we will be copying objects in.
  void SetSpace(space::BumpPointerSpace* space);

  // Initializes internal structures.
  void Init();

  // Find the default mark bitmap.
  void FindDefaultMarkBitmap();

  void ScanObject(mirror::Object* obj)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  // Marks the root set at the start of a garbage collection.
  void MarkRoots()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  // Bind the live bits to the mark bits of bitmaps for spaces that are never collected, ie
  // the image. Mark that portion of the heap as immune.
  void BindBitmaps() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::heap_bitmap_lock_);

  void UnBindBitmaps()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  void ProcessReferences(Thread* self) EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Sweeps unmarked objects to complete the garbage collection.
  void Sweep(bool swap_bitmaps) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Sweeps unmarked objects to complete the garbage collection.
  void SweepLargeObjects(bool swap_bitmaps) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  void SweepSystemWeaks()
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  static void MarkRootCallback(mirror::Object** root, void* arg, uint32_t /*tid*/,
                               RootType /*root_type*/)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  static mirror::Object* MarkObjectCallback(mirror::Object* root, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  static void MarkHeapReferenceCallback(mirror::HeapReference<mirror::Object>* obj_ptr, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  static bool HeapReferenceMarkedCallback(mirror::HeapReference<mirror::Object>* ref_ptr,
                                          void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  static void ProcessMarkStackCallback(void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  static void DelayReferenceReferentCallback(mirror::Class* klass, mirror::Reference* ref,
                                             void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  // Schedules an unmarked object for reference processing.
  void DelayReferenceReferent(mirror::Class* klass, mirror::Reference* reference)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

 protected:
  // Returns null if the object is not marked, otherwise returns the forwarding address (same as
  // object for non movable things).
  mirror::Object* GetMarkedForwardAddress(mirror::Object* object) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  static mirror::Object* MarkedForwardingAddressCallback(mirror::Object* object, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Marks or unmarks a large object based on whether or not set is true. If set is true, then we
  // mark, otherwise we unmark.
  bool MarkLargeObject(const mirror::Object* obj)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Expand mark stack to 2x its current size.
  void ResizeMarkStack(size_t new_size);

  // Returns true if we should sweep the space.
  bool ShouldSweepSpace(space::ContinuousSpace* space) const;

  // Push an object onto the mark stack.
  void MarkStackPush(mirror::Object* obj);

  void UpdateAndMarkModUnion()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Recursively blackens objects on the mark stack.
  void ProcessMarkStack()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  // 3 pass mark compact approach.
  void Compact() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  // Calculate the forwarding address of objects marked as "live" in the objects_before_forwarding
  // bitmap.
  void CalculateObjectForwardingAddresses()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  // Update the references of objects by using the forwarding addresses.
  void UpdateReferences() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  static void UpdateRootCallback(mirror::Object** root, void* arg, uint32_t /*thread_id*/,
                                 RootType /*root_type*/)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  // Move objects and restore lock words.
  void MoveObjects() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Move a single object to its forward address.
  void MoveObject(mirror::Object* obj, size_t len) EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Mark a single object.
  void MarkObject(mirror::Object* obj) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_,
                                                                Locks::mutator_lock_);
  bool IsMarked(const mirror::Object* obj) const
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  static mirror::Object* IsMarkedCallback(mirror::Object* object, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);
  void ForwardObject(mirror::Object* obj) EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_,
                                                                   Locks::mutator_lock_);
  // Update a single heap reference.
  void UpdateHeapReference(mirror::HeapReference<mirror::Object>* reference)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void UpdateHeapReferenceCallback(mirror::HeapReference<mirror::Object>* reference,
                                          void* arg)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Update all of the references of a single object.
  void UpdateObjectReferences(mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Revoke all the thread-local buffers.
  void RevokeAllThreadLocalBuffers();

  accounting::ObjectStack* mark_stack_;

  // Immune region, every object inside the immune region is assumed to be marked.
  ImmuneRegion immune_region_;

  // Bump pointer space which we are collecting.
  space::BumpPointerSpace* space_;
  // Cached mark bitmap as an optimization.
  accounting::HeapBitmap* mark_bitmap_;

  // The name of the collector.
  std::string collector_name_;

  // The bump pointer in the space where the next forwarding address will be.
  byte* bump_pointer_;
  // How many live objects we have in the space.
  size_t live_objects_in_space_;

  // Bitmap which describes which objects we have to move, need to do / 2 so that we can handle
  // objects which are only 8 bytes.
  std::unique_ptr<accounting::ContinuousSpaceBitmap> objects_before_forwarding_;
  // Bitmap which describes which lock words we need to restore.
  std::unique_ptr<accounting::ContinuousSpaceBitmap> objects_with_lockword_;
  // Which lock words we need to restore as we are moving objects.
  std::deque<LockWord> lock_words_to_restore_;

 private:
  friend class BitmapSetSlowPathVisitor;
  friend class CalculateObjectForwardingAddressVisitor;
  friend class MarkCompactMarkObjectVisitor;
  friend class MoveObjectVisitor;
  friend class UpdateObjectReferencesVisitor;
  friend class UpdateReferenceVisitor;
  DISALLOW_COPY_AND_ASSIGN(MarkCompact);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_H_
