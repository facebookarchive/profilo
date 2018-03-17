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

#ifndef ART_RUNTIME_MIRROR_EMULATED_STACK_FRAME_H_
#define ART_RUNTIME_MIRROR_EMULATED_STACK_FRAME_H_

#include "dex_instruction.h"
#include "method_type.h"
#include "object.h"
#include "stack.h"
#include "string.h"
#include "utils.h"

namespace art {

struct EmulatedStackFrameOffsets;

namespace mirror {

// C++ mirror of dalvik.system.EmulatedStackFrame
class MANAGED EmulatedStackFrame : public Object {
 public:
  // Creates an emulated stack frame whose type is |frame_type| from
  // a shadow frame.
  template <bool is_range>
  static mirror::EmulatedStackFrame* CreateFromShadowFrameAndArgs(
      Thread* self,
      Handle<mirror::MethodType> args_type,
      Handle<mirror::MethodType> frame_type,
      const ShadowFrame& caller_frame,
      const uint32_t first_src_reg,
      const uint32_t (&arg)[Instruction::kMaxVarArgRegs]) REQUIRES_SHARED(Locks::mutator_lock_);

  // Writes the contents of this emulated stack frame to the |callee_frame|
  // whose type is |callee_type|, starting at |first_dest_reg|.
  bool WriteToShadowFrame(
      Thread* self,
      Handle<mirror::MethodType> callee_type,
      const uint32_t first_dest_reg,
      ShadowFrame* callee_frame) REQUIRES_SHARED(Locks::mutator_lock_);

  // Sets |value| to the return value written to this emulated stack frame (if any).
  void GetReturnValue(Thread* self, JValue* value) REQUIRES_SHARED(Locks::mutator_lock_);

  // Sets the return value slot of this emulated stack frame to |value|.
  void SetReturnValue(Thread* self, const JValue& value) REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::MethodType* GetType() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<MethodType>(OFFSET_OF_OBJECT_MEMBER(EmulatedStackFrame, type_));
  }

  mirror::Object* GetReceiver() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetReferences()->Get(0);
  }

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  mirror::ObjectArray<mirror::Object>* GetReferences() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<mirror::ObjectArray<mirror::Object>>(
        OFFSET_OF_OBJECT_MEMBER(EmulatedStackFrame, references_));
  }

  mirror::ByteArray* GetStackFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<mirror::ByteArray>(
        OFFSET_OF_OBJECT_MEMBER(EmulatedStackFrame, stack_frame_));
  }

  static MemberOffset CallsiteTypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(EmulatedStackFrame, callsite_type_));
  }

  static MemberOffset TypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(EmulatedStackFrame, type_));
  }

  static MemberOffset ReferencesOffset() {
    return MemberOffset(OFFSETOF_MEMBER(EmulatedStackFrame, references_));
  }

  static MemberOffset StackFrameOffset() {
    return MemberOffset(OFFSETOF_MEMBER(EmulatedStackFrame, stack_frame_));
  }

  HeapReference<mirror::MethodType> callsite_type_;
  HeapReference<mirror::ObjectArray<mirror::Object>> references_;
  HeapReference<mirror::ByteArray> stack_frame_;
  HeapReference<mirror::MethodType> type_;

  static GcRoot<mirror::Class> static_class_;  // dalvik.system.EmulatedStackFrame.class

  friend struct art::EmulatedStackFrameOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(EmulatedStackFrame);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_EMULATED_STACK_FRAME_H_
