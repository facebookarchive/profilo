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

#ifndef ART_RUNTIME_MIRROR_THROWABLE_H_
#define ART_RUNTIME_MIRROR_THROWABLE_H_

#include "gc_root.h"
#include "object.h"
#include "object_callbacks.h"
#include "string.h"

namespace art {

struct ThrowableOffsets;

namespace mirror {

// C++ mirror of java.lang.Throwable
class MANAGED Throwable : public Object {
 public:
  void SetDetailMessage(String* new_detail_message) SHARED_REQUIRES(Locks::mutator_lock_);

  String* GetDetailMessage() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(Throwable, detail_message_));
  }

  std::string Dump() SHARED_REQUIRES(Locks::mutator_lock_);

  // This is a runtime version of initCause, you shouldn't use it if initCause may have been
  // overridden. Also it asserts rather than throwing exceptions. Currently this is only used
  // in cases like the verifier where the checks cannot fail and initCause isn't overridden.
  void SetCause(Throwable* cause) SHARED_REQUIRES(Locks::mutator_lock_);
  void SetStackState(Object* state) SHARED_REQUIRES(Locks::mutator_lock_);
  bool IsCheckedException() SHARED_REQUIRES(Locks::mutator_lock_);

  static Class* GetJavaLangThrowable() SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(!java_lang_Throwable_.IsNull());
    return java_lang_Throwable_.Read();
  }

  int32_t GetStackDepth() SHARED_REQUIRES(Locks::mutator_lock_);

  static void SetClass(Class* java_lang_Throwable);
  static void ResetClass();
  static void VisitRoots(RootVisitor* visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  Object* GetStackState() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObjectVolatile<Object>(OFFSET_OF_OBJECT_MEMBER(Throwable, backtrace_));
  }
  Object* GetStackTrace() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFieldObjectVolatile<Object>(OFFSET_OF_OBJECT_MEMBER(Throwable, backtrace_));
  }

  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  HeapReference<Object> backtrace_;  // Note this is Java volatile:
  HeapReference<Throwable> cause_;
  HeapReference<String> detail_message_;
  HeapReference<Object> stack_trace_;
  HeapReference<Object> suppressed_exceptions_;

  static GcRoot<Class> java_lang_Throwable_;

  friend struct art::ThrowableOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Throwable);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_THROWABLE_H_
