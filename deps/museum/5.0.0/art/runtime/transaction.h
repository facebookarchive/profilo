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

#ifndef ART_RUNTIME_TRANSACTION_H_
#define ART_RUNTIME_TRANSACTION_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "object_callbacks.h"
#include "offsets.h"
#include "primitive.h"
#include "safe_map.h"

#include <list>
#include <map>

namespace art {
namespace mirror {
class Array;
class Object;
class String;
}
class InternTable;

class Transaction {
 public:
  Transaction();
  ~Transaction();

  // Record object field changes.
  void RecordWriteField32(mirror::Object* obj, MemberOffset field_offset, uint32_t value,
                          bool is_volatile)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWriteField64(mirror::Object* obj, MemberOffset field_offset, uint64_t value,
                          bool is_volatile)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWriteFieldReference(mirror::Object* obj, MemberOffset field_offset,
                                 mirror::Object* value, bool is_volatile)
      LOCKS_EXCLUDED(log_lock_);

  // Record array change.
  void RecordWriteArray(mirror::Array* array, size_t index, uint64_t value)
      LOCKS_EXCLUDED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Record intern string table changes.
  void RecordStrongStringInsertion(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWeakStringInsertion(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      LOCKS_EXCLUDED(log_lock_);
  void RecordStrongStringRemoval(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      LOCKS_EXCLUDED(log_lock_);
  void RecordWeakStringRemoval(mirror::String* s)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      LOCKS_EXCLUDED(log_lock_);

  // Abort transaction by undoing all recorded changes.
  void Abort()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(log_lock_);

  void VisitRoots(RootCallback* callback, void* arg)
      LOCKS_EXCLUDED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  class ObjectLog {
   public:
    void Log32BitsValue(MemberOffset offset, uint32_t value, bool is_volatile);
    void Log64BitsValue(MemberOffset offset, uint64_t value, bool is_volatile);
    void LogReferenceValue(MemberOffset offset, mirror::Object* obj, bool is_volatile);

    void Undo(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    void VisitRoots(RootCallback* callback, void* arg);

    size_t Size() const {
      return field_values_.size();
    }

   private:
    enum FieldValueKind {
      k32Bits,
      k64Bits,
      kReference
    };
    struct FieldValue {
      // TODO use JValue instead ?
      uint64_t value;
      FieldValueKind kind;
      bool is_volatile;
    };

    void UndoFieldWrite(mirror::Object* obj, MemberOffset field_offset,
                        const FieldValue& field_value) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    // Maps field's offset to its value.
    std::map<uint32_t, FieldValue> field_values_;
  };

  class ArrayLog {
   public:
    void LogValue(size_t index, uint64_t value);

    void Undo(mirror::Array* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    size_t Size() const {
      return array_values_.size();
    }

   private:
    void UndoArrayWrite(mirror::Array* array, Primitive::Type array_type, size_t index,
                        uint64_t value) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    // Maps index to value.
    // TODO use JValue instead ?
    std::map<size_t, uint64_t> array_values_;
  };

  class InternStringLog {
   public:
    enum StringKind {
      kStrongString,
      kWeakString
    };
    enum StringOp {
      kInsert,
      kRemove
    };
    InternStringLog(mirror::String* s, StringKind kind, StringOp op)
      : str_(s), string_kind_(kind), string_op_(op) {
      DCHECK(s != nullptr);
    }

    void Undo(InternTable* intern_table)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
        EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
    void VisitRoots(RootCallback* callback, void* arg);

   private:
    mirror::String* str_;
    StringKind string_kind_;
    StringOp string_op_;
  };

  void LogInternedString(InternStringLog& log)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      LOCKS_EXCLUDED(log_lock_);

  void UndoObjectModifications()
      EXCLUSIVE_LOCKS_REQUIRED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void UndoArrayModifications()
      EXCLUSIVE_LOCKS_REQUIRED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void UndoInternStringTableModifications()
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VisitObjectLogs(RootCallback* callback, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void VisitArrayLogs(RootCallback* callback, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void VisitStringLogs(RootCallback* callback, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(log_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Mutex log_lock_ ACQUIRED_AFTER(Locks::intern_table_lock_);
  std::map<mirror::Object*, ObjectLog> object_logs_ GUARDED_BY(log_lock_);
  std::map<mirror::Array*, ArrayLog> array_logs_  GUARDED_BY(log_lock_);
  std::list<InternStringLog> intern_string_logs_ GUARDED_BY(log_lock_);

  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

}  // namespace art

#endif  // ART_RUNTIME_TRANSACTION_H_
