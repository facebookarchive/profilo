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

#ifndef ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_H_
#define ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_H_

#include "bitmap.h"
#include "base/allocator.h"
#include "card_table.h"
#include "globals.h"
#include "object_callbacks.h"
#include "safe_map.h"

#include <set>
#include <vector>

namespace art {
namespace mirror {
  class Object;
}  // namespace mirror

namespace gc {

namespace collector {
  class MarkSweep;
}  // namespace collector
namespace space {
  class ContinuousSpace;
  class Space;
}  // namespace space

class Heap;

namespace accounting {

class Bitmap;
class HeapBitmap;

// The mod-union table is the union of modified cards. It is used to allow the card table to be
// cleared between GC phases, reducing the number of dirty cards that need to be scanned.
class ModUnionTable {
 public:
  typedef std::set<uint8_t*, std::less<uint8_t*>,
                   TrackingAllocator<uint8_t*, kAllocatorTagModUnionCardSet>> CardSet;
  typedef MemoryRangeBitmap<CardTable::kCardSize> CardBitmap;

  explicit ModUnionTable(const std::string& name, Heap* heap, space::ContinuousSpace* space)
      : name_(name),
        heap_(heap),
        space_(space) {
  }

  virtual ~ModUnionTable() {}

  // Clear cards which map to a memory range of a space. This doesn't immediately update the
  // mod-union table, as updating the mod-union table may have an associated cost, such as
  // determining references to track.
  virtual void ClearCards() = 0;

  // Set all the cards.
  virtual void SetCards() = 0;

  // Update the mod-union table using data stored by ClearCards. There may be multiple ClearCards
  // before a call to update, for example, back-to-back sticky GCs. Also mark references to other
  // spaces which are stored in the mod-union table.
  virtual void UpdateAndMarkReferences(MarkHeapReferenceCallback* callback, void* arg) = 0;

  // Verification, sanity checks that we don't have clean cards which conflict with out cached data
  // for said cards. Exclusive lock is required since verify sometimes uses
  // SpaceBitmap::VisitMarkedRange and VisitMarkedRange can't know if the callback will modify the
  // bitmap or not.
  virtual void Verify() EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_) = 0;

  // Returns true if a card is marked inside the mod union table. Used for testing. The address
  // doesn't need to be aligned.
  virtual bool ContainsCardFor(uintptr_t addr) = 0;

  virtual void Dump(std::ostream& os) = 0;
  space::ContinuousSpace* GetSpace() {
    return space_;
  }
  Heap* GetHeap() const {
    return heap_;
  }
  const std::string& GetName() const {
    return name_;
  }

 protected:
  const std::string name_;
  Heap* const heap_;
  space::ContinuousSpace* const space_;
};

// Reference caching implementation. Caches references pointing to alloc space(s) for each card.
class ModUnionTableReferenceCache : public ModUnionTable {
 public:
  explicit ModUnionTableReferenceCache(const std::string& name, Heap* heap,
                                       space::ContinuousSpace* space)
      : ModUnionTable(name, heap, space) {}
  virtual ~ModUnionTableReferenceCache() {}

  // Clear and store cards for a space.
  void ClearCards() OVERRIDE;

  // Update table based on cleared cards and mark all references to the other spaces.
  void UpdateAndMarkReferences(MarkHeapReferenceCallback* callback, void* arg) OVERRIDE
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Exclusive lock is required since verify uses SpaceBitmap::VisitMarkedRange and
  // VisitMarkedRange can't know if the callback will modify the bitmap or not.
  void Verify() OVERRIDE
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_);

  // Function that tells whether or not to add a reference to the table.
  virtual bool ShouldAddReference(const mirror::Object* ref) const = 0;

  virtual bool ContainsCardFor(uintptr_t addr) OVERRIDE;

  virtual void Dump(std::ostream& os) OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  virtual void SetCards() OVERRIDE;

 protected:
  // Cleared card array, used to update the mod-union table.
  ModUnionTable::CardSet cleared_cards_;

  // Maps from dirty cards to their corresponding alloc space references.
  AllocationTrackingSafeMap<const uint8_t*, std::vector<mirror::HeapReference<mirror::Object>*>,
                            kAllocatorTagModUnionReferenceArray> references_;
};

// Card caching implementation. Keeps track of which cards we cleared and only this information.
class ModUnionTableCardCache : public ModUnionTable {
 public:
  // Note: There is assumption that the space End() doesn't change.
  explicit ModUnionTableCardCache(const std::string& name, Heap* heap,
                                  space::ContinuousSpace* space);
  virtual ~ModUnionTableCardCache() {}

  // Clear and store cards for a space.
  virtual void ClearCards() OVERRIDE;

  // Mark all references to the alloc space(s).
  virtual void UpdateAndMarkReferences(MarkHeapReferenceCallback* callback, void* arg) OVERRIDE
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Nothing to verify.
  virtual void Verify() OVERRIDE {}

  virtual void Dump(std::ostream& os) OVERRIDE;

  virtual bool ContainsCardFor(uintptr_t addr) OVERRIDE;

  // Sets all the cards in the mod union table to be marked.
  virtual void SetCards() OVERRIDE;

 protected:
  // Cleared card bitmap, used to update the mod-union table.
  std::unique_ptr<CardBitmap> card_bitmap_;
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_MOD_UNION_TABLE_H_
