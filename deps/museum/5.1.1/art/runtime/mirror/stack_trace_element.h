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

#ifndef ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_H_
#define ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_H_

#include "gc_root.h"
#include "object.h"
#include "object_callbacks.h"

namespace art {

template<class T> class Handle;
struct StackTraceElementOffsets;

namespace mirror {

// C++ mirror of java.lang.StackTraceElement
class MANAGED StackTraceElement FINAL : public Object {
 public:
  String* GetDeclaringClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, declaring_class_));
  }

  String* GetMethodName() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, method_name_));
  }

  String* GetFileName() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, file_name_));
  }

  int32_t GetLineNumber() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, line_number_));
  }

  static StackTraceElement* Alloc(Thread* self, Handle<String> declaring_class,
                                  Handle<String> method_name, Handle<String> file_name,
                                  int32_t line_number)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void SetClass(Class* java_lang_StackTraceElement);
  static void ResetClass();
  static void VisitRoots(RootCallback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static Class* GetStackTraceElement() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(!java_lang_StackTraceElement_.IsNull());
    return java_lang_StackTraceElement_.Read();
  }

 private:
  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  HeapReference<String> declaring_class_;
  HeapReference<String> file_name_;
  HeapReference<String> method_name_;
  int32_t line_number_;

  template<bool kTransactionActive>
  void Init(Handle<String> declaring_class, Handle<String> method_name, Handle<String> file_name,
            int32_t line_number)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static GcRoot<Class> java_lang_StackTraceElement_;

  friend struct art::StackTraceElementOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(StackTraceElement);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_H_
