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
#include "base/mutex.h"
#include "gc_root.h"
#include "object_callbacks.h"

namespace art {

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

  size_t Size() const;
  size_t StrongSize() const;
  size_t WeakSize() const;

  void VisitRoots(RootCallback* callback, void* arg, VisitRootFlags flags)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void DumpForSigQuit(std::ostream& os) const;

  void DisallowNewInterns() EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  void AllowNewInterns() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  class StringHashEquals {
   public:
    std::size_t operator()(const GcRoot<mirror::String>& root) NO_THREAD_SAFETY_ANALYSIS;
    bool operator()(const GcRoot<mirror::String>& a, const GcRoot<mirror::String>& b)
        NO_THREAD_SAFETY_ANALYSIS;
  };
  typedef std::unordered_set<GcRoot<mirror::String>, StringHashEquals, StringHashEquals,
      TrackingAllocator<GcRoot<mirror::String>, kAllocatorTagInternTable>> Table;

  mirror::String* Insert(mirror::String* s, bool is_strong)
      LOCKS_EXCLUDED(Locks::intern_table_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::String* LookupStrong(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::String* LookupWeak(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::String* Lookup(Table* table, mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::String* InsertStrong(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* InsertWeak(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveStrong(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveWeak(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void Remove(Table* table, mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);

  // Transaction rollback access.
  mirror::String* InsertStrongFromTransaction(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  mirror::String* InsertWeakFromTransaction(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveStrongFromTransaction(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RemoveWeakFromTransaction(mirror::String* s)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  friend class Transaction;

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
