/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_IMT_CONFLICT_TABLE_H_
#define ART_RUNTIME_IMT_CONFLICT_TABLE_H_

#include <cstddef>

#include "base/casts.h"
#include "base/enums.h"
#include "base/macros.h"

namespace art {

class ArtMethod;

// Table to resolve IMT conflicts at runtime. The table is attached to
// the jni entrypoint of IMT conflict ArtMethods.
// The table contains a list of pairs of { interface_method, implementation_method }
// with the last entry being null to make an assembly implementation of a lookup
// faster.
class ImtConflictTable {
  enum MethodIndex {
    kMethodInterface,
    kMethodImplementation,
    kMethodCount,  // Number of elements in enum.
  };

 public:
  // Build a new table copying `other` and adding the new entry formed of
  // the pair { `interface_method`, `implementation_method` }
  ImtConflictTable(ImtConflictTable* other,
                   ArtMethod* interface_method,
                   ArtMethod* implementation_method,
                   PointerSize pointer_size) {
    const size_t count = other->NumEntries(pointer_size);
    for (size_t i = 0; i < count; ++i) {
      SetInterfaceMethod(i, pointer_size, other->GetInterfaceMethod(i, pointer_size));
      SetImplementationMethod(i, pointer_size, other->GetImplementationMethod(i, pointer_size));
    }
    SetInterfaceMethod(count, pointer_size, interface_method);
    SetImplementationMethod(count, pointer_size, implementation_method);
    // Add the null marker.
    SetInterfaceMethod(count + 1, pointer_size, nullptr);
    SetImplementationMethod(count + 1, pointer_size, nullptr);
  }

  // num_entries excludes the header.
  ImtConflictTable(size_t num_entries, PointerSize pointer_size) {
    SetInterfaceMethod(num_entries, pointer_size, nullptr);
    SetImplementationMethod(num_entries, pointer_size, nullptr);
  }

  // Set an entry at an index.
  void SetInterfaceMethod(size_t index, PointerSize pointer_size, ArtMethod* method) {
    SetMethod(index * kMethodCount + kMethodInterface, pointer_size, method);
  }

  void SetImplementationMethod(size_t index, PointerSize pointer_size, ArtMethod* method) {
    SetMethod(index * kMethodCount + kMethodImplementation, pointer_size, method);
  }

  ArtMethod* GetInterfaceMethod(size_t index, PointerSize pointer_size) const {
    return GetMethod(index * kMethodCount + kMethodInterface, pointer_size);
  }

  ArtMethod* GetImplementationMethod(size_t index, PointerSize pointer_size) const {
    return GetMethod(index * kMethodCount + kMethodImplementation, pointer_size);
  }

  void** AddressOfInterfaceMethod(size_t index, PointerSize pointer_size) {
    return AddressOfMethod(index * kMethodCount + kMethodInterface, pointer_size);
  }

  void** AddressOfImplementationMethod(size_t index, PointerSize pointer_size) {
    return AddressOfMethod(index * kMethodCount + kMethodImplementation, pointer_size);
  }

  // Return true if two conflict tables are the same.
  bool Equals(ImtConflictTable* other, PointerSize pointer_size) const {
    size_t num = NumEntries(pointer_size);
    if (num != other->NumEntries(pointer_size)) {
      return false;
    }
    for (size_t i = 0; i < num; ++i) {
      if (GetInterfaceMethod(i, pointer_size) != other->GetInterfaceMethod(i, pointer_size) ||
          GetImplementationMethod(i, pointer_size) !=
              other->GetImplementationMethod(i, pointer_size)) {
        return false;
      }
    }
    return true;
  }

  // Visit all of the entries.
  // NO_THREAD_SAFETY_ANALYSIS for calling with held locks. Visitor is passed a pair of ArtMethod*
  // and also returns one. The order is <interface, implementation>.
  template<typename Visitor>
  void Visit(const Visitor& visitor, PointerSize pointer_size) NO_THREAD_SAFETY_ANALYSIS {
    uint32_t table_index = 0;
    for (;;) {
      ArtMethod* interface_method = GetInterfaceMethod(table_index, pointer_size);
      if (interface_method == nullptr) {
        break;
      }
      ArtMethod* implementation_method = GetImplementationMethod(table_index, pointer_size);
      auto input = std::make_pair(interface_method, implementation_method);
      std::pair<ArtMethod*, ArtMethod*> updated = visitor(input);
      if (input.first != updated.first) {
        SetInterfaceMethod(table_index, pointer_size, updated.first);
      }
      if (input.second != updated.second) {
        SetImplementationMethod(table_index, pointer_size, updated.second);
      }
      ++table_index;
    }
  }

  // Lookup the implementation ArtMethod associated to `interface_method`. Return null
  // if not found.
  ArtMethod* Lookup(ArtMethod* interface_method, PointerSize pointer_size) const {
    uint32_t table_index = 0;
    for (;;) {
      ArtMethod* current_interface_method = GetInterfaceMethod(table_index, pointer_size);
      if (current_interface_method == nullptr) {
        break;
      }
      if (current_interface_method == interface_method) {
        return GetImplementationMethod(table_index, pointer_size);
      }
      ++table_index;
    }
    return nullptr;
  }

  // Compute the number of entries in this table.
  size_t NumEntries(PointerSize pointer_size) const {
    uint32_t table_index = 0;
    while (GetInterfaceMethod(table_index, pointer_size) != nullptr) {
      ++table_index;
    }
    return table_index;
  }

  // Compute the size in bytes taken by this table.
  size_t ComputeSize(PointerSize pointer_size) const {
    // Add the end marker.
    return ComputeSize(NumEntries(pointer_size), pointer_size);
  }

  // Compute the size in bytes needed for copying the given `table` and add
  // one more entry.
  static size_t ComputeSizeWithOneMoreEntry(ImtConflictTable* table, PointerSize pointer_size) {
    return table->ComputeSize(pointer_size) + EntrySize(pointer_size);
  }

  // Compute size with a fixed number of entries.
  static size_t ComputeSize(size_t num_entries, PointerSize pointer_size) {
    return (num_entries + 1) * EntrySize(pointer_size);  // Add one for null terminator.
  }

  static size_t EntrySize(PointerSize pointer_size) {
    return static_cast<size_t>(pointer_size) * static_cast<size_t>(kMethodCount);
  }

 private:
  void** AddressOfMethod(size_t index, PointerSize pointer_size) {
    if (pointer_size == PointerSize::k64) {
      return reinterpret_cast<void**>(&data64_[index]);
    } else {
      return reinterpret_cast<void**>(&data32_[index]);
    }
  }

  ArtMethod* GetMethod(size_t index, PointerSize pointer_size) const {
    if (pointer_size == PointerSize::k64) {
      return reinterpret_cast<ArtMethod*>(static_cast<uintptr_t>(data64_[index]));
    } else {
      return reinterpret_cast<ArtMethod*>(static_cast<uintptr_t>(data32_[index]));
    }
  }

  void SetMethod(size_t index, PointerSize pointer_size, ArtMethod* method) {
    if (pointer_size == PointerSize::k64) {
      data64_[index] = dchecked_integral_cast<uint64_t>(reinterpret_cast<uintptr_t>(method));
    } else {
      data32_[index] = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(method));
    }
  }

  // Array of entries that the assembly stubs will iterate over. Note that this is
  // not fixed size, and we allocate data prior to calling the constructor
  // of ImtConflictTable.
  union {
    uint32_t data32_[0];
    uint64_t data64_[0];
  };

  DISALLOW_COPY_AND_ASSIGN(ImtConflictTable);
};

}  // namespace art

#endif  // ART_RUNTIME_IMT_CONFLICT_TABLE_H_
