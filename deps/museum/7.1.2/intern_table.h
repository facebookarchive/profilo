/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERN_TABLE_H_
#define ART_RUNTIME_INTERN_TABLE_H_

#include <unordered_set>

#include "atomic.h"
#include "base/allocator.h"
#include "base/hash_set.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "gc/weak_root_state.h"
#include "object_callbacks.h"

namespace art {

namespace gc {
namespace space {
class ImageSpace;
}  // namespace space
}  // namespace gc

enum VisitRootFlags : uint8_t;

namespace mirror {
class String;
}  // namespace mirror
class Transaction;

/**
 * Used to intern strings.
 *
 * There are actually two tables: one that holds strong references to its strings, and one that
 * holds weak references. The former is used for string literals, for which there is an effective
 * reference from the constant pool. The latter is used for strings interned at runtime via
 * String.intern. Some code (XML parsers being a prime example) relies on being able to intern
 * arbitrarily many strings for the duration of a parse without permanently increasing the memory
 * footprint.
 */
class InternTable {
 public:
  InternTable();

  // Interns a potentially new string in the 'strong' table. May cause thread suspension.
  mirror::String* InternStrong(int32_t utf16_length, const char* utf8_data)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // Only used by image writer. Special version that may not cause thread suspension since the GC
  // cannot be running while we are doing image writing. Maybe be called while while holding a
  // lock since there will not be thread suspension.
  mirror::String* InternStrongImageString(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Interns a potentially new string in the 'strong' table. May cause thread suspension.
  mirror::String* InternStrong(const char* utf8_data) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  // Interns a potentially new string in the 'strong' table. May cause thread suspension.
  mirror::String* InternStrong(mirror::String* s) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  // Interns a potentially new string in the 'weak' table. May cause thread suspension.
  mirror::String* InternWeak(mirror::String* s) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  void SweepInternTableWeaks(IsMarkedVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::intern_table_lock_);

  bool ContainsWeak(mirror::String* s) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::intern_table_lock_);

  // Lookup a strong intern, returns null if not found.
  mirror::String* LookupStrong(Thread* self, mirror::String* s)
      REQUIRES(!Locks::intern_table_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::String* LookupStrong(Thread* self, uint32_t utf16_length, const char* utf8_data)
      REQUIRES(!Locks::intern_table_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Lookup a weak intern, returns null if not found.
  mirror::String* LookupWeak(Thread* self, mirror::String* s)
      REQUIRES(!Locks::intern_table_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Total number of interned strings.
  size_t Size() const REQUIRES(!Locks::intern_table_lock_);

  // Total number of weakly live interned strings.
  size_t StrongSize() const REQUIRES(!Locks::intern_table_lock_);

  // Total number of strongly live interned strings.
  size_t WeakSize() const REQUIRES(!Locks::intern_table_lock_);

  void VisitRoots(RootVisitor* visitor, VisitRootFlags flags)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Locks::intern_table_lock_);

  void DumpForSigQuit(std::ostream& os) const REQUIRES(!Locks::intern_table_lock_);

  void BroadcastForNewInterns() SHARED_REQUIRES(Locks::mutator_lock_);

  // Adds all of the resolved image strings from the image spaces into the intern table. The
  // advantage of doing this is preventing expensive DexFile::FindStringId calls. Sets
  // images_added_to_intern_table_ to true.
  void AddImagesStringsToTable(const std::vector<gc::space::ImageSpace*>& image_spaces)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Locks::intern_table_lock_);

  // Add a new intern table for inserting to, previous intern tables are still there but no
  // longer inserted into and ideally unmodified. This is done to prevent dirty pages.
  void AddNewTable()
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!Locks::intern_table_lock_);

  // Read the intern table from memory. The elements aren't copied, the intern hash set data will
  // point to somewhere within ptr. Only reads the strong interns.
  size_t AddTableFromMemory(const uint8_t* ptr) REQUIRES(!Locks::intern_table_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Write the post zygote intern table to a pointer. Only writes the strong interns since it is
  // expected that there is no weak interns since this is called from the image writer.
  size_t WriteToMemory(uint8_t* ptr) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::intern_table_lock_);

  // Change the weak root state. May broadcast to waiters.
  void ChangeWeakRootState(gc::WeakRootState new_state)
      REQUIRES(!Locks::intern_table_lock_);

 private:
  // Modified UTF-8-encoded string treated as UTF16.
  class Utf8String {
   public:
    Utf8String(uint32_t utf16_length, const char* utf8_data, int32_t hash)
        : hash_(hash), utf16_length_(utf16_length), utf8_data_(utf8_data) { }

    int32_t GetHash() const { return hash_; }
    uint32_t GetUtf16Length() const { return utf16_length_; }
    const char* GetUtf8Data() const { return utf8_data_; }

   private:
    int32_t hash_;
    uint32_t utf16_length_;
    const char* utf8_data_;
  };

  class StringHashEquals {
   public:
    std::size_t operator()(const GcRoot<mirror::String>& root) const NO_THREAD_SAFETY_ANALYSIS;
    bool operator()(const GcRoot<mirror::String>& a, const GcRoot<mirror::String>& b) const
        NO_THREAD_SAFETY_ANALYSIS;

    // Utf8String can be used for lookup.
    std::size_t operator()(const Utf8String& key) const { return key.GetHash(); }
    bool operator()(const GcRoot<mirror::String>& a, const Utf8String& b) const
        NO_THREAD_SAFETY_ANALYSIS;
  };
  class GcRootEmptyFn {
   public:
    void MakeEmpty(GcRoot<mirror::String>& item) const {
      item = GcRoot<mirror::String>();
    }
    bool IsEmpty(const GcRoot<mirror::String>& item) const {
      return item.IsNull();
    }
  };

  // Table which holds pre zygote and post zygote interned strings. There is one instance for
  // weak interns and strong interns.
  class Table {
   public:
    Table();
    mirror::String* Find(mirror::String* s) SHARED_REQUIRES(Locks::mutator_lock_)
        REQUIRES(Locks::intern_table_lock_);
    mirror::String* Find(const Utf8String& string) SHARED_REQUIRES(Locks::mutator_lock_)
        REQUIRES(Locks::intern_table_lock_);
    void Insert(mirror::String* s) SHARED_REQUIRES(Locks::mutator_lock_)
        REQUIRES(Locks::intern_table_lock_);
    void Remove(mirror::String* s)
        SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    void VisitRoots(RootVisitor* visitor)
        SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    void SweepWeaks(IsMarkedVisitor* visitor)
        SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    // Add a new intern table that will only be inserted into from now on.
    void AddNewTable() REQUIRES(Locks::intern_table_lock_);
    size_t Size() const REQUIRES(Locks::intern_table_lock_);
    // Read and add an intern table from ptr.
    // Tables read are inserted at the front of the table array. Only checks for conflicts in
    // debug builds. Returns how many bytes were read.
    size_t AddTableFromMemory(const uint8_t* ptr)
        REQUIRES(Locks::intern_table_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
    // Write the intern tables to ptr, if there are multiple tables they are combined into a single
    // one. Returns how many bytes were written.
    size_t WriteToMemory(uint8_t* ptr)
        REQUIRES(Locks::intern_table_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

   private:
    typedef HashSet<GcRoot<mirror::String>, GcRootEmptyFn, StringHashEquals, StringHashEquals,
        TrackingAllocator<GcRoot<mirror::String>, kAllocatorTagInternTable>> UnorderedSet;

    void SweepWeaks(UnorderedSet* set, IsMarkedVisitor* visitor)
        SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

    // We call AddNewTable when we create the zygote to reduce private dirty pages caused by
    // modifying the zygote intern table. The back of table is modified when strings are interned.
    std::vector<UnorderedSet> tables_;
  };

  // Insert if non null, otherwise return null. Must be called holding the mutator lock.
  // If holding_locks is true, then we may also hold other locks. If holding_locks is true, then we
  // require GC is not running since it is not safe to wait while holding locks.
  mirror::String* Insert(mirror::String* s, bool is_strong, bool holding_locks)
      REQUIRES(!Locks::intern_table_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::String* LookupStrongLocked(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  mirror::String* LookupWeakLocked(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  mirror::String* InsertStrong(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  mirror::String* InsertWeak(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  void RemoveStrong(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  void RemoveWeak(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  // Transaction rollback access.
  mirror::String* LookupStringFromImage(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  mirror::String* InsertStrongFromTransaction(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  mirror::String* InsertWeakFromTransaction(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  void RemoveStrongFromTransaction(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  void RemoveWeakFromTransaction(mirror::String* s)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  size_t AddTableFromMemoryLocked(const uint8_t* ptr)
      REQUIRES(Locks::intern_table_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  // Change the weak root state. May broadcast to waiters.
  void ChangeWeakRootStateLocked(gc::WeakRootState new_state)
      REQUIRES(Locks::intern_table_lock_);

  // Wait until we can read weak roots.
  void WaitUntilAccessible(Thread* self)
      REQUIRES(Locks::intern_table_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  bool images_added_to_intern_table_ GUARDED_BY(Locks::intern_table_lock_);
  bool log_new_roots_ GUARDED_BY(Locks::intern_table_lock_);
  ConditionVariable weak_intern_condition_ GUARDED_BY(Locks::intern_table_lock_);
  // Since this contains (strong) roots, they need a read barrier to
  // enable concurrent intern table (strong) root scan. Do not
  // directly access the strings in it. Use functions that contain
  // read barriers.
  Table strong_interns_ GUARDED_BY(Locks::intern_table_lock_);
  std::vector<GcRoot<mirror::String>> new_strong_intern_roots_
      GUARDED_BY(Locks::intern_table_lock_);
  // Since this contains (weak) roots, they need a read barrier. Do
  // not directly access the strings in it. Use functions that contain
  // read barriers.
  Table weak_interns_ GUARDED_BY(Locks::intern_table_lock_);
  // Weak root state, used for concurrent system weak processing and more.
  gc::WeakRootState weak_root_state_ GUARDED_BY(Locks::intern_table_lock_);

  friend class Transaction;
  DISALLOW_COPY_AND_ASSIGN(InternTable);
};

}  // namespace art

#endif  // ART_RUNTIME_INTERN_TABLE_H_
