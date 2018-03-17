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

#ifndef ART_RUNTIME_COMMON_DEX_OPERATIONS_H_
#define ART_RUNTIME_COMMON_DEX_OPERATIONS_H_

#include "art_field.h"
#include "art_method.h"
#include "class_linker.h"
#include "interpreter/unstarted_runtime.h"
#include "runtime.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace interpreter {
  void ArtInterpreterToInterpreterBridge(Thread* self,
                                        const DexFile::CodeItem* code_item,
                                        ShadowFrame* shadow_frame,
                                        JValue* result)
     REQUIRES_SHARED(Locks::mutator_lock_);

  void ArtInterpreterToCompiledCodeBridge(Thread* self,
                                          ArtMethod* caller,
                                          const DexFile::CodeItem* code_item,
                                          ShadowFrame* shadow_frame,
                                          JValue* result);
}  // namespace interpreter

inline void PerformCall(Thread* self,
                        const DexFile::CodeItem* code_item,
                        ArtMethod* caller_method,
                        const size_t first_dest_reg,
                        ShadowFrame* callee_frame,
                        JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (LIKELY(Runtime::Current()->IsStarted())) {
    ArtMethod* target = callee_frame->GetMethod();
    if (ClassLinker::ShouldUseInterpreterEntrypoint(
        target,
        target->GetEntryPointFromQuickCompiledCode())) {
      interpreter::ArtInterpreterToInterpreterBridge(self, code_item, callee_frame, result);
    } else {
      interpreter::ArtInterpreterToCompiledCodeBridge(
          self, caller_method, code_item, callee_frame, result);
    }
  } else {
    interpreter::UnstartedRuntime::Invoke(self, code_item, callee_frame, result, first_dest_reg);
  }
}

template<Primitive::Type field_type>
static ALWAYS_INLINE void DoFieldGetCommon(Thread* self,
                                           const ShadowFrame& shadow_frame,
                                           ObjPtr<mirror::Object> obj,
                                           ArtField* field,
                                           JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  field->GetDeclaringClass()->AssertInitializedOrInitializingInThread(self);

  // Report this field access to instrumentation if needed.
  instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
  if (UNLIKELY(instrumentation->HasFieldReadListeners())) {
    StackHandleScope<1> hs(self);
    // Wrap in handle wrapper in case the listener does thread suspension.
    HandleWrapperObjPtr<mirror::Object> h(hs.NewHandleWrapper(&obj));
    ObjPtr<mirror::Object> this_object;
    if (!field->IsStatic()) {
      this_object = obj;
    }
    instrumentation->FieldReadEvent(self,
                                    this_object.Ptr(),
                                    shadow_frame.GetMethod(),
                                    shadow_frame.GetDexPC(),
                                    field);
  }

  switch (field_type) {
    case Primitive::kPrimBoolean:
      result->SetZ(field->GetBoolean(obj));
      break;
    case Primitive::kPrimByte:
      result->SetB(field->GetByte(obj));
      break;
    case Primitive::kPrimChar:
      result->SetC(field->GetChar(obj));
      break;
    case Primitive::kPrimShort:
      result->SetS(field->GetShort(obj));
      break;
    case Primitive::kPrimInt:
      result->SetI(field->GetInt(obj));
      break;
    case Primitive::kPrimLong:
      result->SetJ(field->GetLong(obj));
      break;
    case Primitive::kPrimNot:
      result->SetL(field->GetObject(obj));
      break;
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable " << field_type;
      break;
  }
}

template<Primitive::Type field_type, bool do_assignability_check, bool transaction_active>
ALWAYS_INLINE bool DoFieldPutCommon(Thread* self,
                                    const ShadowFrame& shadow_frame,
                                    ObjPtr<mirror::Object> obj,
                                    ArtField* field,
                                    const JValue& value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  field->GetDeclaringClass()->AssertInitializedOrInitializingInThread(self);

  // Report this field access to instrumentation if needed. Since we only have the offset of
  // the field from the base of the object, we need to look for it first.
  instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
  if (UNLIKELY(instrumentation->HasFieldWriteListeners())) {
    StackHandleScope<1> hs(self);
    // Wrap in handle wrapper in case the listener does thread suspension.
    HandleWrapperObjPtr<mirror::Object> h(hs.NewHandleWrapper(&obj));
    ObjPtr<mirror::Object> this_object = field->IsStatic() ? nullptr : obj;
    instrumentation->FieldWriteEvent(self, this_object.Ptr(),
                                     shadow_frame.GetMethod(),
                                     shadow_frame.GetDexPC(),
                                     field,
                                     value);
  }

  switch (field_type) {
    case Primitive::kPrimBoolean:
      field->SetBoolean<transaction_active>(obj, value.GetZ());
      break;
    case Primitive::kPrimByte:
      field->SetByte<transaction_active>(obj, value.GetB());
      break;
    case Primitive::kPrimChar:
      field->SetChar<transaction_active>(obj, value.GetC());
      break;
    case Primitive::kPrimShort:
      field->SetShort<transaction_active>(obj, value.GetS());
      break;
    case Primitive::kPrimInt:
      field->SetInt<transaction_active>(obj, value.GetI());
      break;
    case Primitive::kPrimLong:
      field->SetLong<transaction_active>(obj, value.GetJ());
      break;
    case Primitive::kPrimNot: {
      ObjPtr<mirror::Object> reg = value.GetL();
      if (do_assignability_check && reg != nullptr) {
        // FieldHelper::GetType can resolve classes, use a handle wrapper which will restore the
        // object in the destructor.
        ObjPtr<mirror::Class> field_class;
        {
          StackHandleScope<2> hs(self);
          HandleWrapperObjPtr<mirror::Object> h_reg(hs.NewHandleWrapper(&reg));
          HandleWrapperObjPtr<mirror::Object> h_obj(hs.NewHandleWrapper(&obj));
          field_class = field->GetType<true>();
        }
        if (!reg->VerifierInstanceOf(field_class.Ptr())) {
          // This should never happen.
          std::string temp1, temp2, temp3;
          self->ThrowNewExceptionF("Ljava/lang/InternalError;",
                                   "Put '%s' that is not instance of field '%s' in '%s'",
                                   reg->GetClass()->GetDescriptor(&temp1),
                                   field_class->GetDescriptor(&temp2),
                                   field->GetDeclaringClass()->GetDescriptor(&temp3));
          return false;
        }
      }
      field->SetObj<transaction_active>(obj, reg);
      break;
    }
    case Primitive::kPrimVoid: {
      LOG(FATAL) << "Unreachable " << field_type;
      break;
    }
  }
  return true;
}

}  // namespace art

#endif  // ART_RUNTIME_COMMON_DEX_OPERATIONS_H_
