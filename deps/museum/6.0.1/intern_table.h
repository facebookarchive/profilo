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

#include "base/allocator.h"
#include "base/hash_set.h"
#include "base/mutex.h"
#include "gc_root.h"
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

  // Interns a potentially new string in the 'strong' table. (See above.)
  mirror::String* InternStrong(int32_t utf16_length, const char* utf8_data)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Interns a potentially new string in the 'strong' table. (See above.)
  mirror::String* InternStrong(const char* utf8_data)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Interns a potentially new string in the 'strong' table. (See above.)
  mirror::String* InternStrong(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Interns a potentially new string in the 'weak' table. (See above.)
  mirror::String* InternWeak(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SweepInternTableWeaks(IsMarkedCallback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool ContainsWeak(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Total number of interned strings.
  size_t Size() const LOCKS_EXCLUDED(Locks::intern_table_lock_);
  // Total number of weakly live interned strings.
  size_t StrongSize() const LOCKS_EXCLUDED(Locks::intern_table_lock_);
  // Total number of strongly live interned strings.
  size_t WeakSize() const LOCKS_EXCLUDED(Locks::intern_table_lock_);

  void VisitRoots(RootVisitor* visitor, VisitRootFlags flags)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void DumpForSigQuit(std::ostream& os) const;

  void DisallowNewInterns() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void AllowNewInterns() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void EnsureNewInternsDisallowed() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Adds all of the resolved image strings from the image space into the intern table. The
  // advantage of doing this is preventing expensive DexFile::FindStringId calls.
  void AddImageStringsToTable(gc::space::ImageSpace* image_space)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(Locks::intern_table_lock_);
  // Copy the post zygote tables to pre zygote to save memory by preventing dirty pages.
  void SwapPostZygoteWithPreZygote()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(Locks::intern_table_lock_);

  // Add an intern table which was serialized to the image.
  void AddImageInternTable(gc::space::ImageSpace* image_space)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(Locks::intern_table_lock_);

  // Read the intern table from memory. The elements aren't copied, the intern hash set data will
  // point to somewhere within ptr. Only reads the strong interns.
  size_t ReadFromMemory(const uint8_t* ptr) LOCKS_EXCLUDED(Locks::intern_table_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Write the post zygote intern table to a pointer. Only writes the strong interns since it is
  // expected that there is no weak interns since this is called from the image writer.
  size_t WriteToMemory(uint8_t* ptr) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::intern_table_lock_);

 private:
  class StringHashEquals {
   public:
    std::size_t operator()(const GcRoot<mirror::String>& root) const NO_THREAD_SAFETY_ANALYSIS;
    bool operator()(const GcRoot<mirror::String>& a, const GcRoot<mirror::String>& b) const
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
    mirror::String* Find(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    void Insert(mirror::String* s) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    void Remove(mirror::String* s)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    void VisitRoots(RootVisitor* visitor)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    void SweepWeaks(IsMarkedCallback* callback, void* arg)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    void SwapPostZygoteWithPreZygote() EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    size_t Size() const EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    // Read pre zygote table is called from ReadFromMemory which happens during runtime creation
    // when we load the image intern table. Returns how many bytes were read.
    size_t ReadIntoPreZygoteTable(const uint8_t* ptr)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    // The image writer calls WritePostZygoteTable through WriteToMemory, it writes the interns in
    // the post zygote table. Returns how many bytes were written.
    size_t WriteFromPostZygoteTable(uint8_t* ptr)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

   private:
    typedef HashSet<GcRoot<mirror::String>, GcRootEmptyFn, StringHashEquals, StringHashEquals,
        TrackingAllocator<GcRoot<mirror::String>, kAllocatorTagInternTable>> UnorderedSet;

    void SweepWeaks(UnorderedSet* set, IsMarkedCallback* callback, void* arg)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);

    // We call SwapPostZygoteWithPreZygote when we create the zygote to reduce private dirty pages
    // caused by modifying the zygote intern table hash table. The pre zygote table are the
    // interned strings which were interned before we created the zygote space. Post zygote is self
    // explanatory.
    UnorderedSet pre_zygote_table_;
    UnorderedSet post_zygote_table_;
  };

  // Insert if non null, otherwise return null.
  mirror::String* Insert(mirror::String* s, bool is_strong)
      LOCKS_EXCLUDED(Locks::intern_table_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::String* LookupStrong(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* LookupWeak(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* InsertStrong(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* InsertWeak(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveStrong(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveWeak(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);

  // Transaction rollback access.
  mirror::String* LookupStringFromImage(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* InsertStrongFromTransaction(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* InsertWeakFromTransaction(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveStrongFromTransaction(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveWeakFromTransaction(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  friend class Transaction;

  size_t ReadFromMemoryLocked(const uint8_t* ptr)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool image_added_to_intern_table_ GUARDED_BY(Locks::intern_table_lock_);
  bool log_new_roots_ GUARDED_BY(Locks::intern_table_lock_);
  bool allow_new_interns_ GUARDED_BY(Locks::intern_table_lock_);
  ConditionVariable new_intern_condition_ GUARDED_BY(Locks::intern_table_lock_);
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
};

}  // namespace art

#endif  // ART_RUNTIME_INTERN_TABLE_H_
