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
#include "base/value_object.h"
#include "gc_root.h"
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

class Transaction FINAL {
 public:
  static constexpr const char* kAbortExceptionDescriptor = "dalvik.system.TransactionAbortError";
  static constexpr const char* kAbortExceptionSignature = "Ldalvik/system/TransactionAbortError;";

  Transaction();
  ~Transaction();

  void Abort(const std::string& abort_message)
      REQUIRES(!log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void ThrowAbortError(Thread* self, const std::string* abort_message)
      REQUIRES(!log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool IsAborted() REQUIRES(!log_lock_);

  // Record object field changes.
  void RecordWriteFieldBoolean(mirror::Object* obj, MemberOffset field_offset, uint8_t value,
                               bool is_volatile)
      REQUIRES(!log_lock_);
  void RecordWriteFieldByte(mirror::Object* obj, MemberOffset field_offset, int8_t value,
                               bool is_volatile)
      REQUIRES(!log_lock_);
  void RecordWriteFieldChar(mirror::Object* obj, MemberOffset field_offset, uint16_t value,
                            bool is_volatile)
      REQUIRES(!log_lock_);
  void RecordWriteFieldShort(mirror::Object* obj, MemberOffset field_offset, int16_t value,
                             bool is_volatile)
      REQUIRES(!log_lock_);
  void RecordWriteField32(mirror::Object* obj, MemberOffset field_offset, uint32_t value,
                          bool is_volatile)
      REQUIRES(!log_lock_);
  void RecordWriteField64(mirror::Object* obj, MemberOffset field_offset, uint64_t value,
                          bool is_volatile)
      REQUIRES(!log_lock_);
  void RecordWriteFieldReference(mirror::Object* obj, MemberOffset field_offset,
                                 mirror::Object* value, bool is_volatile)
      REQUIRES(!log_lock_);

  // Record array change.
  void RecordWriteArray(mirror::Array* array, size_t index, uint64_t value)
      REQUIRES(!log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Record intern string table changes.
  void RecordStrongStringInsertion(mirror::String* s)
      REQUIRES(Locks::intern_table_lock_)
      REQUIRES(!log_lock_);
  void RecordWeakStringInsertion(mirror::String* s)
      REQUIRES(Locks::intern_table_lock_)
      REQUIRES(!log_lock_);
  void RecordStrongStringRemoval(mirror::String* s)
      REQUIRES(Locks::intern_table_lock_)
      REQUIRES(!log_lock_);
  void RecordWeakStringRemoval(mirror::String* s)
      REQUIRES(Locks::intern_table_lock_)
      REQUIRES(!log_lock_);

  // Abort transaction by undoing all recorded changes.
  void Rollback()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!log_lock_);

  void VisitRoots(RootVisitor* visitor)
      REQUIRES(!log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  class ObjectLog : public ValueObject {
   public:
    void LogBooleanValue(MemberOffset offset, uint8_t value, bool is_volatile);
    void LogByteValue(MemberOffset offset, int8_t value, bool is_volatile);
    void LogCharValue(MemberOffset offset, uint16_t value, bool is_volatile);
    void LogShortValue(MemberOffset offset, int16_t value, bool is_volatile);
    void Log32BitsValue(MemberOffset offset, uint32_t value, bool is_volatile);
    void Log64BitsValue(MemberOffset offset, uint64_t value, bool is_volatile);
    void LogReferenceValue(MemberOffset offset, mirror::Object* obj, bool is_volatile);

    void Undo(mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_);
    void VisitRoots(RootVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_);

    size_t Size() const {
      return field_values_.size();
    }

   private:
    enum FieldValueKind {
      kBoolean,
      kByte,
      kChar,
      kShort,
      k32Bits,
      k64Bits,
      kReference
    };
    struct FieldValue : public ValueObject {
      // TODO use JValue instead ?
      uint64_t value;
      FieldValueKind kind;
      bool is_volatile;
    };

    void LogValue(FieldValueKind kind, MemberOffset offset, uint64_t value, bool is_volatile);
    void UndoFieldWrite(mirror::Object* obj, MemberOffset field_offset,
                        const FieldValue& field_value) SHARED_REQUIRES(Locks::mutator_lock_);

    // Maps field's offset to its value.
    std::map<uint32_t, FieldValue> field_values_;
  };

  class ArrayLog : public ValueObject {
   public:
    void LogValue(size_t index, uint64_t value);

    void Undo(mirror::Array* obj) SHARED_REQUIRES(Locks::mutator_lock_);

    size_t Size() const {
      return array_values_.size();
    }

   private:
    void UndoArrayWrite(mirror::Array* array, Primitive::Type array_type, size_t index,
                        uint64_t value) SHARED_REQUIRES(Locks::mutator_lock_);

    // Maps index to value.
    // TODO use JValue instead ?
    std::map<size_t, uint64_t> array_values_;
  };

  class InternStringLog : public ValueObject {
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
        SHARED_REQUIRES(Locks::mutator_lock_)
        REQUIRES(Locks::intern_table_lock_);
    void VisitRoots(RootVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_);

   private:
    mirror::String* str_;
    const StringKind string_kind_;
    const StringOp string_op_;
  };

  void LogInternedString(const InternStringLog& log)
      REQUIRES(Locks::intern_table_lock_)
      REQUIRES(!log_lock_);

  void UndoObjectModifications()
      REQUIRES(log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void UndoArrayModifications()
      REQUIRES(log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void UndoInternStringTableModifications()
      REQUIRES(Locks::intern_table_lock_)
      REQUIRES(log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void VisitObjectLogs(RootVisitor* visitor)
      REQUIRES(log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void VisitArrayLogs(RootVisitor* visitor)
      REQUIRES(log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void VisitStringLogs(RootVisitor* visitor)
      REQUIRES(log_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  const std::string& GetAbortMessage() REQUIRES(!log_lock_);

  Mutex log_lock_ ACQUIRED_AFTER(Locks::intern_table_lock_);
  std::map<mirror::Object*, ObjectLog> object_logs_ GUARDED_BY(log_lock_);
  std::map<mirror::Array*, ArrayLog> array_logs_  GUARDED_BY(log_lock_);
  std::list<InternStringLog> intern_string_logs_ GUARDED_BY(log_lock_);
  bool aborted_ GUARDED_BY(log_lock_);
  std::string abort_message_ GUARDED_BY(log_lock_);

  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

}  // namespace art

#endif  // ART_RUNTIME_TRANSACTION_H_
