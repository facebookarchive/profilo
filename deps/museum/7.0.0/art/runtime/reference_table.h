/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_REFERENCE_TABLE_H_
#define ART_RUNTIME_REFERENCE_TABLE_H_

#include <museum/7.0.0/external/libcxx/cstddef>
#include <museum/7.0.0/external/libcxx/iosfwd>
#include <museum/7.0.0/external/libcxx/string>
#include <museum/7.0.0/external/libcxx/vector>

#include <museum/7.0.0/art/runtime/base/allocator.h>
#include <museum/7.0.0/art/runtime/base/mutex.h>
#include <museum/7.0.0/art/runtime/gc_root.h>
#include <museum/7.0.0/art/runtime/object_callbacks.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace mirror {
class Object;
}  // namespace mirror

// Maintain a table of references.  Used for JNI monitor references and
// JNI pinned array references.
//
// None of the functions are synchronized.
class ReferenceTable {
 public:
  ReferenceTable(const char* name, size_t initial_size, size_t max_size);
  ~ReferenceTable();

  void Add(mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_);

  void Remove(mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_);

  size_t Size() const;

  void Dump(std::ostream& os) SHARED_REQUIRES(Locks::mutator_lock_);

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  typedef std::vector<GcRoot<mirror::Object>,
                      TrackingAllocator<GcRoot<mirror::Object>, kAllocatorTagReferenceTable>> Table;
  static void Dump(std::ostream& os, Table& entries)
      SHARED_REQUIRES(Locks::mutator_lock_);
  friend class IndirectReferenceTable;  // For Dump.

  std::string name_;
  Table entries_;
  size_t max_size_;
};

} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_REFERENCE_TABLE_H_
