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

#ifndef ART_RUNTIME_CLASS_TABLE_INL_H_
#define ART_RUNTIME_CLASS_TABLE_INL_H_

#include "class_table.h"

#include "gc_root-inl.h"
#include "oat_file.h"

namespace art {

template<class Visitor>
void ClassTable::VisitRoots(Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (TableSlot& table_slot : class_set) {
      table_slot.VisitRoot(visitor);
    }
  }
  for (GcRoot<mirror::Object>& root : strong_roots_) {
    visitor.VisitRoot(root.AddressWithoutBarrier());
  }
  for (const OatFile* oat_file : oat_files_) {
    for (GcRoot<mirror::Object>& root : oat_file->GetBssGcRoots()) {
      visitor.VisitRootIfNonNull(root.AddressWithoutBarrier());
    }
  }
}

template<class Visitor>
void ClassTable::VisitRoots(const Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (TableSlot& table_slot : class_set) {
      table_slot.VisitRoot(visitor);
    }
  }
  for (GcRoot<mirror::Object>& root : strong_roots_) {
    visitor.VisitRoot(root.AddressWithoutBarrier());
  }
  for (const OatFile* oat_file : oat_files_) {
    for (GcRoot<mirror::Object>& root : oat_file->GetBssGcRoots()) {
      visitor.VisitRootIfNonNull(root.AddressWithoutBarrier());
    }
  }
}

template <typename Visitor>
bool ClassTable::Visit(Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (TableSlot& table_slot : class_set) {
      if (!visitor(table_slot.Read())) {
        return false;
      }
    }
  }
  return true;
}

template <typename Visitor>
bool ClassTable::Visit(const Visitor& visitor) {
  ReaderMutexLock mu(Thread::Current(), lock_);
  for (ClassSet& class_set : classes_) {
    for (TableSlot& table_slot : class_set) {
      if (!visitor(table_slot.Read())) {
        return false;
      }
    }
  }
  return true;
}

template<ReadBarrierOption kReadBarrierOption>
inline mirror::Class* ClassTable::TableSlot::Read() const {
  const uint32_t before = data_.LoadRelaxed();
  ObjPtr<mirror::Class> const before_ptr(ExtractPtr(before));
  ObjPtr<mirror::Class> const after_ptr(
      GcRoot<mirror::Class>(before_ptr).Read<kReadBarrierOption>());
  if (kReadBarrierOption != kWithoutReadBarrier && before_ptr != after_ptr) {
    // If another thread raced and updated the reference, do not store the read barrier updated
    // one.
    data_.CompareExchangeStrongRelease(before, Encode(after_ptr, MaskHash(before)));
  }
  return after_ptr.Ptr();
}

template<typename Visitor>
inline void ClassTable::TableSlot::VisitRoot(const Visitor& visitor) const {
  const uint32_t before = data_.LoadRelaxed();
  ObjPtr<mirror::Class> before_ptr(ExtractPtr(before));
  GcRoot<mirror::Class> root(before_ptr);
  visitor.VisitRoot(root.AddressWithoutBarrier());
  ObjPtr<mirror::Class> after_ptr(root.Read<kWithoutReadBarrier>());
  if (before_ptr != after_ptr) {
    // If another thread raced and updated the reference, do not store the read barrier updated
    // one.
    data_.CompareExchangeStrongRelease(before, Encode(after_ptr, MaskHash(before)));
  }
}

inline ObjPtr<mirror::Class> ClassTable::TableSlot::ExtractPtr(uint32_t data) {
  return reinterpret_cast<mirror::Class*>(data & ~kHashMask);
}

inline uint32_t ClassTable::TableSlot::Encode(ObjPtr<mirror::Class> klass, uint32_t hash_bits) {
  DCHECK_LE(hash_bits, kHashMask);
  return reinterpret_cast<uintptr_t>(klass.Ptr()) | hash_bits;
}

inline ClassTable::TableSlot::TableSlot(ObjPtr<mirror::Class> klass, uint32_t descriptor_hash)
    : data_(Encode(klass, MaskHash(descriptor_hash))) {
  if (kIsDebugBuild) {
    std::string temp;
    const uint32_t hash = ComputeModifiedUtf8Hash(klass->GetDescriptor(&temp));
    CHECK_EQ(descriptor_hash, hash);
  }
}

template <typename Filter>
inline void ClassTable::RemoveStrongRoots(const Filter& filter) {
  WriterMutexLock mu(Thread::Current(), lock_);
  strong_roots_.erase(std::remove_if(strong_roots_.begin(), strong_roots_.end(), filter),
                      strong_roots_.end());
}

}  // namespace art

#endif  // ART_RUNTIME_CLASS_TABLE_INL_H_
