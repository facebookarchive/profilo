/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_CLASS_TABLE_H_
#define ART_RUNTIME_CLASS_TABLE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/allocator.h"
#include "base/hash_set.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "gc_root.h"
#include "obj_ptr.h"
#include "object_callbacks.h"
#include "runtime.h"

namespace art {

class OatFile;

namespace mirror {
  class ClassLoader;
}  // namespace mirror

// Each loader has a ClassTable
class ClassTable {
 public:
  class TableSlot {
   public:
    TableSlot() : data_(0u) {}

    TableSlot(const TableSlot& copy) : data_(copy.data_.LoadRelaxed()) {}

    explicit TableSlot(ObjPtr<mirror::Class> klass);

    TableSlot(ObjPtr<mirror::Class> klass, uint32_t descriptor_hash);

    TableSlot& operator=(const TableSlot& copy) {
      data_.StoreRelaxed(copy.data_.LoadRelaxed());
      return *this;
    }

    bool IsNull() const REQUIRES_SHARED(Locks::mutator_lock_) {
      return Read<kWithoutReadBarrier>() == nullptr;
    }

    uint32_t Hash() const {
      return MaskHash(data_.LoadRelaxed());
    }

    static uint32_t MaskHash(uint32_t hash) {
      return hash & kHashMask;
    }

    bool MaskedHashEquals(uint32_t other) const {
      return MaskHash(other) == Hash();
    }

    static uint32_t HashDescriptor(ObjPtr<mirror::Class> klass)
        REQUIRES_SHARED(Locks::mutator_lock_);

    template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
    mirror::Class* Read() const REQUIRES_SHARED(Locks::mutator_lock_);

    // NO_THREAD_SAFETY_ANALYSIS since the visitor may require heap bitmap lock.
    template<typename Visitor>
    void VisitRoot(const Visitor& visitor) const NO_THREAD_SAFETY_ANALYSIS;

   private:
    // Extract a raw pointer from an address.
    static ObjPtr<mirror::Class> ExtractPtr(uint32_t data)
        REQUIRES_SHARED(Locks::mutator_lock_);

    static uint32_t Encode(ObjPtr<mirror::Class> klass, uint32_t hash_bits)
        REQUIRES_SHARED(Locks::mutator_lock_);

    // Data contains the class pointer GcRoot as well as the low bits of the descriptor hash.
    mutable Atomic<uint32_t> data_;
    static const uint32_t kHashMask = kObjectAlignment - 1;
  };

  using DescriptorHashPair = std::pair<const char*, uint32_t>;

  class ClassDescriptorHashEquals {
   public:
    // uint32_t for cross compilation.
    uint32_t operator()(const TableSlot& slot) const NO_THREAD_SAFETY_ANALYSIS;
    // Same class loader and descriptor.
    bool operator()(const TableSlot& a, const TableSlot& b) const
        NO_THREAD_SAFETY_ANALYSIS;
    // Same descriptor.
    bool operator()(const TableSlot& a, const DescriptorHashPair& b) const
        NO_THREAD_SAFETY_ANALYSIS;
    // uint32_t for cross compilation.
    uint32_t operator()(const DescriptorHashPair& pair) const NO_THREAD_SAFETY_ANALYSIS;
  };

  class TableSlotEmptyFn {
   public:
    void MakeEmpty(TableSlot& item) const NO_THREAD_SAFETY_ANALYSIS {
      item = TableSlot();
      DCHECK(IsEmpty(item));
    }
    bool IsEmpty(const TableSlot& item) const NO_THREAD_SAFETY_ANALYSIS {
      return item.IsNull();
    }
  };

  // Hash set that hashes class descriptor, and compares descriptors and class loaders. Results
  // should be compared for a matching class descriptor and class loader.
  typedef HashSet<TableSlot,
                  TableSlotEmptyFn,
                  ClassDescriptorHashEquals,
                  ClassDescriptorHashEquals,
                  TrackingAllocator<TableSlot, kAllocatorTagClassTable>> ClassSet;

  ClassTable();

  // Used by image writer for checking.
  bool Contains(ObjPtr<mirror::Class> klass)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Freeze the current class tables by allocating a new table and never updating or modifying the
  // existing table. This helps prevents dirty pages after caused by inserting after zygote fork.
  void FreezeSnapshot()
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of classes in previous snapshots defined by `defining_loader`.
  size_t NumZygoteClasses(ObjPtr<mirror::ClassLoader> defining_loader) const
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns all off the classes in the lastest snapshot defined by `defining_loader`.
  size_t NumNonZygoteClasses(ObjPtr<mirror::ClassLoader> defining_loader) const
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of classes in previous snapshots no matter the defining loader.
  size_t NumReferencedZygoteClasses() const
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns all off the classes in the lastest snapshot no matter the defining loader.
  size_t NumReferencedNonZygoteClasses() const
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Update a class in the table with the new class. Returns the existing class which was replaced.
  mirror::Class* UpdateClass(const char* descriptor, mirror::Class* new_klass, size_t hash)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // NO_THREAD_SAFETY_ANALYSIS for object marking requiring heap bitmap lock.
  template<class Visitor>
  void VisitRoots(Visitor& visitor)
      NO_THREAD_SAFETY_ANALYSIS
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<class Visitor>
  void VisitRoots(const Visitor& visitor)
      NO_THREAD_SAFETY_ANALYSIS
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Stops visit if the visitor returns false.
  template <typename Visitor>
  bool Visit(Visitor& visitor)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template <typename Visitor>
  bool Visit(const Visitor& visitor)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return the first class that matches the descriptor. Returns null if there are none.
  mirror::Class* Lookup(const char* descriptor, size_t hash)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return the first class that matches the descriptor of klass. Returns null if there are none.
  mirror::Class* LookupByDescriptor(ObjPtr<mirror::Class> klass)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Try to insert a class and return the inserted class if successful. If another class
  // with the same descriptor is already in the table, return the existing entry.
  ObjPtr<mirror::Class> TryInsert(ObjPtr<mirror::Class> klass)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void Insert(ObjPtr<mirror::Class> klass)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void InsertWithHash(ObjPtr<mirror::Class> klass, size_t hash)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if the class was found and removed, false otherwise.
  bool Remove(const char* descriptor)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if we inserted the strong root, false if it already exists.
  bool InsertStrongRoot(ObjPtr<mirror::Object> obj)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if we inserted the oat file, false if it already exists.
  bool InsertOatFile(const OatFile* oat_file)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Combines all of the tables into one class set.
  size_t WriteToMemory(uint8_t* ptr) const
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Read a table from ptr and put it at the front of the class set.
  size_t ReadFromMemory(uint8_t* ptr)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Add a class set to the front of classes.
  void AddClassSet(ClassSet&& set)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Clear strong roots (other than classes themselves).
  void ClearStrongRoots()
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ReaderWriterMutex& GetLock() {
    return lock_;
  }

 private:
  // Only copies classes.
  void CopyWithoutLocks(const ClassTable& source_table) NO_THREAD_SAFETY_ANALYSIS;
  void InsertWithoutLocks(ObjPtr<mirror::Class> klass) NO_THREAD_SAFETY_ANALYSIS;

  size_t CountDefiningLoaderClasses(ObjPtr<mirror::ClassLoader> defining_loader,
                                    const ClassSet& set) const
      REQUIRES(lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if we inserted the oat file, false if it already exists.
  bool InsertOatFileLocked(const OatFile* oat_file)
      REQUIRES(lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Lock to guard inserting and removing.
  mutable ReaderWriterMutex lock_;
  // We have a vector to help prevent dirty pages after the zygote forks by calling FreezeSnapshot.
  std::vector<ClassSet> classes_ GUARDED_BY(lock_);
  // Extra strong roots that can be either dex files or dex caches. Dex files used by the class
  // loader which may not be owned by the class loader must be held strongly live. Also dex caches
  // are held live to prevent them being unloading once they have classes in them.
  std::vector<GcRoot<mirror::Object>> strong_roots_ GUARDED_BY(lock_);
  // Keep track of oat files with GC roots associated with dex caches in `strong_roots_`.
  std::vector<const OatFile*> oat_files_ GUARDED_BY(lock_);

  friend class ImageWriter;  // for InsertWithoutLocks.
};

}  // namespace art

#endif  // ART_RUNTIME_CLASS_TABLE_H_
