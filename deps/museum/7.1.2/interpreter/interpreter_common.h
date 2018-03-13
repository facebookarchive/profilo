/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_
#define ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_

#include "interpreter.h"

#include <math.h>

#include <iostream>
#include <sstream>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "handle_scope-inl.h"
#include "jit/jit.h"
#include "lambda/art_lambda_method.h"
#include "lambda/box_table.h"
#include "lambda/closure.h"
#include "lambda/closure_builder-inl.h"
#include "lambda/leaking_allocator.h"
#include "lambda/shorty_field_type.h"
#include "mirror/class-inl.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "stack.h"
#include "thread.h"
#include "well_known_classes.h"

using ::art::ArtMethod;
using ::art::mirror::Array;
using ::art::mirror::BooleanArray;
using ::art::mirror::ByteArray;
using ::art::mirror::CharArray;
using ::art::mirror::Class;
using ::art::mirror::ClassLoader;
using ::art::mirror::IntArray;
using ::art::mirror::LongArray;
using ::art::mirror::Object;
using ::art::mirror::ObjectArray;
using ::art::mirror::ShortArray;
using ::art::mirror::String;
using ::art::mirror::Throwable;

namespace art {
namespace interpreter {

// External references to all interpreter implementations.

template<bool do_access_check, bool transaction_active>
extern JValue ExecuteSwitchImpl(Thread* self, const DexFile::CodeItem* code_item,
                                ShadowFrame& shadow_frame, JValue result_register,
                                bool interpret_one_instruction);

template<bool do_access_check, bool transaction_active>
extern JValue ExecuteGotoImpl(Thread* self, const DexFile::CodeItem* code_item,
                              ShadowFrame& shadow_frame, JValue result_register);

// Mterp does not support transactions or access check, thus no templated versions.
extern "C" bool ExecuteMterpImpl(Thread* self, const DexFile::CodeItem* code_item,
                                 ShadowFrame* shadow_frame, JValue* result_register);

void ThrowNullPointerExceptionFromInterpreter()
    SHARED_REQUIRES(Locks::mutator_lock_);

template <bool kMonitorCounting>
static inline void DoMonitorEnter(Thread* self,
                                  ShadowFrame* frame,
                                  Object* ref)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_) {
  StackHandleScope<1> hs(self);
  Handle<Object> h_ref(hs.NewHandle(ref));
  h_ref->MonitorEnter(self);
  if (kMonitorCounting && frame->GetMethod()->MustCountLocks()) {
    frame->GetLockCountData().AddMonitor(self, h_ref.Get());
  }
}

template <bool kMonitorCounting>
static inline void DoMonitorExit(Thread* self,
                                 ShadowFrame* frame,
                                 Object* ref)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_) {
  StackHandleScope<1> hs(self);
  Handle<Object> h_ref(hs.NewHandle(ref));
  h_ref->MonitorExit(self);
  if (kMonitorCounting && frame->GetMethod()->MustCountLocks()) {
    frame->GetLockCountData().RemoveMonitorOrThrow(self, h_ref.Get());
  }
}

template <bool kMonitorCounting>
static inline bool DoMonitorCheckOnExit(Thread* self, ShadowFrame* frame)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_) {
  if (kMonitorCounting && frame->GetMethod()->MustCountLocks()) {
    return frame->GetLockCountData().CheckAllMonitorsReleasedOrThrow(self);
  }
  return true;
}

void AbortTransactionF(Thread* self, const char* fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)))
    SHARED_REQUIRES(Locks::mutator_lock_);

void AbortTransactionV(Thread* self, const char* fmt, va_list args)
    SHARED_REQUIRES(Locks::mutator_lock_);

void RecordArrayElementsInTransaction(mirror::Array* array, int32_t count)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Invokes the given method. This is part of the invocation support and is used by DoInvoke and
// DoInvokeVirtualQuick functions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range, bool do_assignability_check>
bool DoCall(ArtMethod* called_method, Thread* self, ShadowFrame& shadow_frame,
            const Instruction* inst, uint16_t inst_data, JValue* result);

// Invokes the given lambda closure. This is part of the invocation support and is used by
// DoLambdaInvoke functions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range, bool do_assignability_check>
bool DoLambdaCall(ArtMethod* called_method, Thread* self, ShadowFrame& shadow_frame,
                  const Instruction* inst, uint16_t inst_data, JValue* result);

// Validates that the art method corresponding to a lambda method target
// is semantically valid:
//
// Must be ACC_STATIC and ACC_LAMBDA. Must be a concrete managed implementation
// (i.e. not native, not proxy, not abstract, ...).
//
// If the validation fails, return false and raise an exception.
static inline bool IsValidLambdaTargetOrThrow(ArtMethod* called_method)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  bool success = false;

  if (UNLIKELY(called_method == nullptr)) {
    // The shadow frame should already be pushed, so we don't need to update it.
  } else if (UNLIKELY(!called_method->IsInvokable())) {
    called_method->ThrowInvocationTimeError();
    // We got an error.
    // TODO(iam): Also handle the case when the method is non-static, what error do we throw?
    // TODO(iam): Also make sure that ACC_LAMBDA is set.
  } else if (UNLIKELY(called_method->GetCodeItem() == nullptr)) {
    // Method could be native, proxy method, etc. Lambda targets have to be concrete impls,
    // so don't allow this.
  } else {
    success = true;
  }

  return success;
}

// Write out the 'Closure*' into vreg and vreg+1, as if it was a jlong.
static inline void WriteLambdaClosureIntoVRegs(ShadowFrame& shadow_frame,
                                               const lambda::Closure& lambda_closure,
                                               uint32_t vreg) {
  // Split the method into a lo and hi 32 bits so we can encode them into 2 virtual registers.
  uint32_t closure_lo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&lambda_closure));
  uint32_t closure_hi = static_cast<uint32_t>(reinterpret_cast<uint64_t>(&lambda_closure)
                                                    >> BitSizeOf<uint32_t>());
  // Use uint64_t instead of uintptr_t to allow shifting past the max on 32-bit.
  static_assert(sizeof(uint64_t) >= sizeof(uintptr_t), "Impossible");

  DCHECK_NE(closure_lo | closure_hi, 0u);

  shadow_frame.SetVReg(vreg, closure_lo);
  shadow_frame.SetVReg(vreg + 1, closure_hi);
}

// Handles create-lambda instructions.
// Returns true on success, otherwise throws an exception and returns false.
// (Exceptions are thrown by creating a new exception and then being put in the thread TLS)
//
// The closure must be allocated big enough to hold the data, and should not be
// pre-initialized. It is initialized with the actual captured variables as a side-effect,
// although this should be unimportant to the caller since this function also handles storing it to
// the ShadowFrame.
//
// As a work-in-progress implementation, this shoves the ArtMethod object corresponding
// to the target dex method index into the target register vA and vA + 1.
template<bool do_access_check>
static inline bool DoCreateLambda(Thread* self,
                                  const Instruction* inst,
                                  /*inout*/ShadowFrame& shadow_frame,
                                  /*inout*/lambda::ClosureBuilder* closure_builder,
                                  /*inout*/lambda::Closure* uninitialized_closure) {
  DCHECK(closure_builder != nullptr);
  DCHECK(uninitialized_closure != nullptr);
  DCHECK_ALIGNED(uninitialized_closure, alignof(lambda::Closure));

  using lambda::ArtLambdaMethod;
  using lambda::LeakingAllocator;

  /*
   * create-lambda is opcode 0x21c
   * - vA is the target register where the closure will be stored into
   *   (also stores into vA + 1)
   * - vB is the method index which will be the target for a later invoke-lambda
   */
  const uint32_t method_idx = inst->VRegB_21c();
  mirror::Object* receiver = nullptr;  // Always static. (see 'kStatic')
  ArtMethod* sf_method = shadow_frame.GetMethod();
  ArtMethod* const called_method = FindMethodFromCode<kStatic, do_access_check>(
      method_idx, &receiver, sf_method, self);

  uint32_t vreg_dest_closure = inst->VRegA_21c();

  if (UNLIKELY(!IsValidLambdaTargetOrThrow(called_method))) {
    CHECK(self->IsExceptionPending());
    shadow_frame.SetVReg(vreg_dest_closure, 0u);
    shadow_frame.SetVReg(vreg_dest_closure + 1, 0u);
    return false;
  }

  ArtLambdaMethod* initialized_lambda_method;
  // Initialize the ArtLambdaMethod with the right data.
  {
    // Allocate enough memory to store a well-aligned ArtLambdaMethod.
    // This is not the final type yet since the data starts out uninitialized.
    LeakingAllocator::AlignedMemoryStorage<ArtLambdaMethod>* uninitialized_lambda_method =
            LeakingAllocator::AllocateMemory<ArtLambdaMethod>(self);

    std::string captured_variables_shorty = closure_builder->GetCapturedVariableShortyTypes();
    std::string captured_variables_long_type_desc;

    // Synthesize a long type descriptor from the short one.
    for (char shorty : captured_variables_shorty) {
      lambda::ShortyFieldType shorty_field_type(shorty);
      if (shorty_field_type.IsObject()) {
        // Not the true type, but good enough until we implement verifier support.
        captured_variables_long_type_desc += "Ljava/lang/Object;";
        UNIMPLEMENTED(FATAL) << "create-lambda with an object captured variable";
      } else if (shorty_field_type.IsLambda()) {
        // Not the true type, but good enough until we implement verifier support.
        captured_variables_long_type_desc += "Ljava/lang/Runnable;";
        UNIMPLEMENTED(FATAL) << "create-lambda with a lambda captured variable";
      } else {
        // The primitive types have the same length shorty or not, so this is always correct.
        DCHECK(shorty_field_type.IsPrimitive());
        captured_variables_long_type_desc += shorty_field_type;
      }
    }

    // Copy strings to dynamically allocated storage. This leaks, but that's ok. Fix it later.
    // TODO: Strings need to come from the DexFile, so they won't need their own allocations.
    char* captured_variables_type_desc = LeakingAllocator::MakeFlexibleInstance<char>(
        self,
        captured_variables_long_type_desc.size() + 1);
    strcpy(captured_variables_type_desc, captured_variables_long_type_desc.c_str());
    char* captured_variables_shorty_copy = LeakingAllocator::MakeFlexibleInstance<char>(
        self,
        captured_variables_shorty.size() + 1);
    strcpy(captured_variables_shorty_copy, captured_variables_shorty.c_str());

    // After initialization, the object at the storage is well-typed. Use strong type going forward.
    initialized_lambda_method =
        new (uninitialized_lambda_method) ArtLambdaMethod(called_method,
                                                          captured_variables_type_desc,
                                                          captured_variables_shorty_copy,
                                                          true);  // innate lambda
  }

  // Write all the closure captured variables and the closure header into the closure.
  lambda::Closure* initialized_closure =
      closure_builder->CreateInPlace(uninitialized_closure, initialized_lambda_method);

  WriteLambdaClosureIntoVRegs(/*inout*/shadow_frame, *initialized_closure, vreg_dest_closure);
  return true;
}

// Reads out the 'ArtMethod*' stored inside of vreg and vreg+1
//
// Validates that the art method points to a valid lambda function, otherwise throws
// an exception and returns null.
// (Exceptions are thrown by creating a new exception and then being put in the thread TLS)
static inline lambda::Closure* ReadLambdaClosureFromVRegsOrThrow(ShadowFrame& shadow_frame,
                                                                 uint32_t vreg)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  // Lambda closures take up a consecutive pair of 2 virtual registers.
  // On 32-bit the high bits are always 0.
  uint32_t vc_value_lo = shadow_frame.GetVReg(vreg);
  uint32_t vc_value_hi = shadow_frame.GetVReg(vreg + 1);

  uint64_t vc_value_ptr = (static_cast<uint64_t>(vc_value_hi) << BitSizeOf<uint32_t>())
                           | vc_value_lo;

  // Use uint64_t instead of uintptr_t to allow left-shifting past the max on 32-bit.
  static_assert(sizeof(uint64_t) >= sizeof(uintptr_t), "Impossible");
  lambda::Closure* const lambda_closure = reinterpret_cast<lambda::Closure*>(vc_value_ptr);
  DCHECK_ALIGNED(lambda_closure, alignof(lambda::Closure));

  // Guard against the user passing a null closure, which is odd but (sadly) semantically valid.
  if (UNLIKELY(lambda_closure == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    return nullptr;
  } else if (UNLIKELY(!IsValidLambdaTargetOrThrow(lambda_closure->GetTargetMethod()))) {
    // Sanity check against data corruption.
    return nullptr;
  }

  return lambda_closure;
}

// Forward declaration for lock annotations. See below for documentation.
template <bool do_access_check>
static inline const char* GetStringDataByDexStringIndexOrThrow(ShadowFrame& shadow_frame,
                                                               uint32_t string_idx)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Find the c-string data corresponding to a dex file's string index.
// Otherwise, returns null if not found and throws a VerifyError.
//
// Note that with do_access_check=false, we never return null because the verifier
// must guard against invalid string indices.
// (Exceptions are thrown by creating a new exception and then being put in the thread TLS)
template <bool do_access_check>
static inline const char* GetStringDataByDexStringIndexOrThrow(ShadowFrame& shadow_frame,
                                                               uint32_t string_idx) {
  ArtMethod* method = shadow_frame.GetMethod();
  const DexFile* dex_file = method->GetDexFile();

  mirror::Class* declaring_class = method->GetDeclaringClass();
  if (!do_access_check) {
    // MethodVerifier refuses methods with string_idx out of bounds.
    DCHECK_LT(string_idx, declaring_class->GetDexCache()->NumStrings());
  } else {
    // Access checks enabled: perform string index bounds ourselves.
    if (string_idx >= dex_file->GetHeader().string_ids_size_) {
      ThrowVerifyError(declaring_class, "String index '%" PRIu32 "' out of bounds",
                       string_idx);
      return nullptr;
    }
  }

  const char* type_string = dex_file->StringDataByIdx(string_idx);

  if (UNLIKELY(type_string == nullptr)) {
    CHECK_EQ(false, do_access_check)
        << " verifier should've caught invalid string index " << string_idx;
    CHECK_EQ(true, do_access_check)
        << " string idx size check should've caught invalid string index " << string_idx;
  }

  return type_string;
}

// Handles capture-variable instructions.
// Returns true on success, otherwise throws an exception and returns false.
// (Exceptions are thrown by creating a new exception and then being put in the thread TLS)
template<bool do_access_check>
static inline bool DoCaptureVariable(Thread* self,
                                     const Instruction* inst,
                                     /*inout*/ShadowFrame& shadow_frame,
                                     /*inout*/lambda::ClosureBuilder* closure_builder) {
  DCHECK(closure_builder != nullptr);
  using lambda::ShortyFieldType;
  /*
   * capture-variable is opcode 0xf6, fmt 0x21c
   * - vA is the source register of the variable that will be captured
   * - vB is the string ID of the variable's type that will be captured
   */
  const uint32_t source_vreg = inst->VRegA_21c();
  const uint32_t string_idx = inst->VRegB_21c();
  // TODO: this should be a proper [type id] instead of a [string ID] pointing to a type.

  const char* type_string = GetStringDataByDexStringIndexOrThrow<do_access_check>(shadow_frame,
                                                                                  string_idx);
  if (UNLIKELY(type_string == nullptr)) {
    CHECK(self->IsExceptionPending());
    return false;
  }

  char type_first_letter = type_string[0];
  ShortyFieldType shorty_type;
  if (do_access_check &&
      UNLIKELY(!ShortyFieldType::MaybeCreate(type_first_letter, /*out*/&shorty_type))) {  // NOLINT: [whitespace/comma] [3]
    ThrowVerifyError(shadow_frame.GetMethod()->GetDeclaringClass(),
                     "capture-variable vB must be a valid type");
    return false;
  } else {
    // Already verified that the type is valid.
    shorty_type = ShortyFieldType(type_first_letter);
  }

  const size_t captured_variable_count = closure_builder->GetCaptureCount();

  // Note: types are specified explicitly so that the closure is packed tightly.
  switch (shorty_type) {
    case ShortyFieldType::kBoolean: {
      uint32_t primitive_narrow_value = shadow_frame.GetVReg(source_vreg);
      closure_builder->CaptureVariablePrimitive<bool>(primitive_narrow_value);
      break;
    }
    case ShortyFieldType::kByte: {
      uint32_t primitive_narrow_value = shadow_frame.GetVReg(source_vreg);
      closure_builder->CaptureVariablePrimitive<int8_t>(primitive_narrow_value);
      break;
    }
    case ShortyFieldType::kChar: {
      uint32_t primitive_narrow_value = shadow_frame.GetVReg(source_vreg);
      closure_builder->CaptureVariablePrimitive<uint16_t>(primitive_narrow_value);
      break;
    }
    case ShortyFieldType::kShort: {
      uint32_t primitive_narrow_value = shadow_frame.GetVReg(source_vreg);
      closure_builder->CaptureVariablePrimitive<int16_t>(primitive_narrow_value);
      break;
    }
    case ShortyFieldType::kInt: {
      uint32_t primitive_narrow_value = shadow_frame.GetVReg(source_vreg);
      closure_builder->CaptureVariablePrimitive<int32_t>(primitive_narrow_value);
      break;
    }
    case ShortyFieldType::kDouble: {
      closure_builder->CaptureVariablePrimitive(shadow_frame.GetVRegDouble(source_vreg));
      break;
    }
    case ShortyFieldType::kFloat: {
      closure_builder->CaptureVariablePrimitive(shadow_frame.GetVRegFloat(source_vreg));
      break;
    }
    case ShortyFieldType::kLambda: {
      UNIMPLEMENTED(FATAL) << " capture-variable with type kLambda";
      // TODO: Capturing lambdas recursively will be done at a later time.
      UNREACHABLE();
    }
    case ShortyFieldType::kLong: {
      closure_builder->CaptureVariablePrimitive(shadow_frame.GetVRegLong(source_vreg));
      break;
    }
    case ShortyFieldType::kObject: {
      closure_builder->CaptureVariableObject(shadow_frame.GetVRegReference(source_vreg));
      UNIMPLEMENTED(FATAL) << " capture-variable with type kObject";
      // TODO: finish implementing this. disabled for now since we can't track lambda refs for GC.
      UNREACHABLE();
    }

    default:
      LOG(FATAL) << "Invalid shorty type value " << shorty_type;
      UNREACHABLE();
  }

  DCHECK_EQ(captured_variable_count + 1, closure_builder->GetCaptureCount());

  return true;
}

// Handles capture-variable instructions.
// Returns true on success, otherwise throws an exception and returns false.
// (Exceptions are thrown by creating a new exception and then being put in the thread TLS)
template<bool do_access_check>
static inline bool DoLiberateVariable(Thread* self,
                                     const Instruction* inst,
                                     size_t captured_variable_index,
                                     /*inout*/ShadowFrame& shadow_frame) {
  using lambda::ShortyFieldType;
  /*
   * liberate-variable is opcode 0xf7, fmt 0x22c
   * - vA is the destination register
   * - vB is the register with the lambda closure in it
   * - vC is the string ID which needs to be a valid field type descriptor
   */

  const uint32_t dest_vreg = inst->VRegA_22c();
  const uint32_t closure_vreg = inst->VRegB_22c();
  const uint32_t string_idx = inst->VRegC_22c();
  // TODO: this should be a proper [type id] instead of a [string ID] pointing to a type.


  // Synthesize a long type descriptor from a shorty type descriptor list.
  // TODO: Fix the dex encoding to contain the long and short type descriptors.
  const char* type_string = GetStringDataByDexStringIndexOrThrow<do_access_check>(shadow_frame,
                                                                                  string_idx);
  if (UNLIKELY(do_access_check && type_string == nullptr)) {
    CHECK(self->IsExceptionPending());
    shadow_frame.SetVReg(dest_vreg, 0);
    return false;
  }

  char type_first_letter = type_string[0];
  ShortyFieldType shorty_type;
  if (do_access_check &&
      UNLIKELY(!ShortyFieldType::MaybeCreate(type_first_letter, /*out*/&shorty_type))) {  // NOLINT: [whitespace/comma] [3]
    ThrowVerifyError(shadow_frame.GetMethod()->GetDeclaringClass(),
                     "liberate-variable vC must be a valid type");
    shadow_frame.SetVReg(dest_vreg, 0);
    return false;
  } else {
    // Already verified that the type is valid.
    shorty_type = ShortyFieldType(type_first_letter);
  }

  // Check for closure being null *after* the type check.
  // This way we can access the type info in case we fail later, to know how many vregs to clear.
  const lambda::Closure* lambda_closure =
      ReadLambdaClosureFromVRegsOrThrow(/*inout*/shadow_frame, closure_vreg);

  // Failed lambda target runtime check, an exception was raised.
  if (UNLIKELY(lambda_closure == nullptr)) {
    CHECK(self->IsExceptionPending());

    // Clear the destination vreg(s) to be safe.
    shadow_frame.SetVReg(dest_vreg, 0);
    if (shorty_type.IsPrimitiveWide() || shorty_type.IsLambda()) {
      shadow_frame.SetVReg(dest_vreg + 1, 0);
    }
    return false;
  }

  if (do_access_check &&
      UNLIKELY(captured_variable_index >= lambda_closure->GetNumberOfCapturedVariables())) {
    ThrowVerifyError(shadow_frame.GetMethod()->GetDeclaringClass(),
                     "liberate-variable captured variable index %zu out of bounds",
                     lambda_closure->GetNumberOfCapturedVariables());
    // Clear the destination vreg(s) to be safe.
    shadow_frame.SetVReg(dest_vreg, 0);
    if (shorty_type.IsPrimitiveWide() || shorty_type.IsLambda()) {
      shadow_frame.SetVReg(dest_vreg + 1, 0);
    }
    return false;
  }

  // Verify that the runtime type of the captured-variable matches the requested dex type.
  if (do_access_check) {
    ShortyFieldType actual_type = lambda_closure->GetCapturedShortyType(captured_variable_index);
    if (actual_type != shorty_type) {
      ThrowVerifyError(shadow_frame.GetMethod()->GetDeclaringClass(),
                     "cannot liberate-variable of runtime type '%c' to dex type '%c'",
                     static_cast<char>(actual_type),
                     static_cast<char>(shorty_type));

      shadow_frame.SetVReg(dest_vreg, 0);
      if (shorty_type.IsPrimitiveWide() || shorty_type.IsLambda()) {
        shadow_frame.SetVReg(dest_vreg + 1, 0);
      }
      return false;
    }

    if (actual_type.IsLambda() || actual_type.IsObject()) {
      UNIMPLEMENTED(FATAL) << "liberate-variable type checks needs to "
                           << "parse full type descriptor for objects and lambdas";
    }
  }

  // Unpack the captured variable from the closure into the correct type, then save it to the vreg.
  if (shorty_type.IsPrimitiveNarrow()) {
    uint32_t primitive_narrow_value =
        lambda_closure->GetCapturedPrimitiveNarrow(captured_variable_index);
    shadow_frame.SetVReg(dest_vreg, primitive_narrow_value);
  } else if (shorty_type.IsPrimitiveWide()) {
      uint64_t primitive_wide_value =
          lambda_closure->GetCapturedPrimitiveWide(captured_variable_index);
      shadow_frame.SetVRegLong(dest_vreg, static_cast<int64_t>(primitive_wide_value));
  } else if (shorty_type.IsObject()) {
    mirror::Object* unpacked_object =
        lambda_closure->GetCapturedObject(captured_variable_index);
    shadow_frame.SetVRegReference(dest_vreg, unpacked_object);

    UNIMPLEMENTED(FATAL) << "liberate-variable cannot unpack objects yet";
  } else if (shorty_type.IsLambda()) {
    UNIMPLEMENTED(FATAL) << "liberate-variable cannot unpack lambdas yet";
  } else {
    LOG(FATAL) << "unreachable";
    UNREACHABLE();
  }

  return true;
}

template<bool do_access_check>
static inline bool DoInvokeLambda(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                                  uint16_t inst_data, JValue* result) {
  /*
   * invoke-lambda is opcode 0x25
   *
   * - vC is the closure register (both vC and vC + 1 will be used to store the closure).
   * - vB is the number of additional registers up to |{vD,vE,vF,vG}| (4)
   * - the rest of the registers are always var-args
   *
   * - reading var-args for 0x25 gets us vD,vE,vF,vG (but not vB)
   */
  uint32_t vreg_closure = inst->VRegC_25x();
  const lambda::Closure* lambda_closure =
      ReadLambdaClosureFromVRegsOrThrow(shadow_frame, vreg_closure);

  // Failed lambda target runtime check, an exception was raised.
  if (UNLIKELY(lambda_closure == nullptr)) {
    CHECK(self->IsExceptionPending());
    result->SetJ(0);
    return false;
  }

  ArtMethod* const called_method = lambda_closure->GetTargetMethod();
  // Invoke a non-range lambda
  return DoLambdaCall<false, do_access_check>(called_method, self, shadow_frame, inst, inst_data,
                                              result);
}

// Handles invoke-XXX/range instructions (other than invoke-lambda[-range]).
// Returns true on success, otherwise throws an exception and returns false.
template<InvokeType type, bool is_range, bool do_access_check>
static inline bool DoInvoke(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                            uint16_t inst_data, JValue* result) {
  const uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  Object* receiver = (type == kStatic) ? nullptr : shadow_frame.GetVRegReference(vregC);
  ArtMethod* sf_method = shadow_frame.GetMethod();
  ArtMethod* const called_method = FindMethodFromCode<type, do_access_check>(
      method_idx, &receiver, sf_method, self);
  // The shadow frame should already be pushed, so we don't need to update it.
  if (UNLIKELY(called_method == nullptr)) {
    CHECK(self->IsExceptionPending());
    result->SetJ(0);
    return false;
  } else if (UNLIKELY(!called_method->IsInvokable())) {
    called_method->ThrowInvocationTimeError();
    result->SetJ(0);
    return false;
  } else {
    jit::Jit* jit = Runtime::Current()->GetJit();
    if (jit != nullptr) {
      if (type == kVirtual || type == kInterface) {
        jit->InvokeVirtualOrInterface(
            self, receiver, sf_method, shadow_frame.GetDexPC(), called_method);
      }
      jit->AddSamples(self, sf_method, 1, /*with_backedges*/false);
    }
    // TODO: Remove the InvokeVirtualOrInterface instrumentation, as it was only used by the JIT.
    if (type == kVirtual || type == kInterface) {
      instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
      if (UNLIKELY(instrumentation->HasInvokeVirtualOrInterfaceListeners())) {
        instrumentation->InvokeVirtualOrInterface(
            self, receiver, sf_method, shadow_frame.GetDexPC(), called_method);
      }
    }
    return DoCall<is_range, do_access_check>(called_method, self, shadow_frame, inst, inst_data,
                                             result);
  }
}

// Handles invoke-virtual-quick and invoke-virtual-quick-range instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range>
static inline bool DoInvokeVirtualQuick(Thread* self, ShadowFrame& shadow_frame,
                                        const Instruction* inst, uint16_t inst_data,
                                        JValue* result) {
  const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  Object* const receiver = shadow_frame.GetVRegReference(vregC);
  if (UNLIKELY(receiver == nullptr)) {
    // We lost the reference to the method index so we cannot get a more
    // precised exception message.
    ThrowNullPointerExceptionFromDexPC();
    return false;
  }
  const uint32_t vtable_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  CHECK(receiver->GetClass()->ShouldHaveEmbeddedVTable());
  ArtMethod* const called_method = receiver->GetClass()->GetEmbeddedVTableEntry(
      vtable_idx, sizeof(void*));
  if (UNLIKELY(called_method == nullptr)) {
    CHECK(self->IsExceptionPending());
    result->SetJ(0);
    return false;
  } else if (UNLIKELY(!called_method->IsInvokable())) {
    called_method->ThrowInvocationTimeError();
    result->SetJ(0);
    return false;
  } else {
    jit::Jit* jit = Runtime::Current()->GetJit();
    if (jit != nullptr) {
      jit->InvokeVirtualOrInterface(
          self, receiver, shadow_frame.GetMethod(), shadow_frame.GetDexPC(), called_method);
      jit->AddSamples(self, shadow_frame.GetMethod(), 1, /*with_backedges*/false);
    }
    instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
    // TODO: Remove the InvokeVirtualOrInterface instrumentation, as it was only used by the JIT.
    if (UNLIKELY(instrumentation->HasInvokeVirtualOrInterfaceListeners())) {
      instrumentation->InvokeVirtualOrInterface(
          self, receiver, shadow_frame.GetMethod(), shadow_frame.GetDexPC(), called_method);
    }
    // No need to check since we've been quickened.
    return DoCall<is_range, false>(called_method, self, shadow_frame, inst, inst_data, result);
  }
}

// Handles iget-XXX and sget-XXX instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check>
bool DoFieldGet(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                uint16_t inst_data) SHARED_REQUIRES(Locks::mutator_lock_);

// Handles iget-quick, iget-wide-quick and iget-object-quick instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<Primitive::Type field_type>
bool DoIGetQuick(ShadowFrame& shadow_frame, const Instruction* inst, uint16_t inst_data)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Handles iput-XXX and sput-XXX instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check,
         bool transaction_active>
bool DoFieldPut(Thread* self, const ShadowFrame& shadow_frame, const Instruction* inst,
                uint16_t inst_data) SHARED_REQUIRES(Locks::mutator_lock_);

// Handles iput-quick, iput-wide-quick and iput-object-quick instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<Primitive::Type field_type, bool transaction_active>
bool DoIPutQuick(const ShadowFrame& shadow_frame, const Instruction* inst, uint16_t inst_data)
    SHARED_REQUIRES(Locks::mutator_lock_);


// Handles string resolution for const-string and const-string-jumbo instructions. Also ensures the
// java.lang.String class is initialized.
static inline String* ResolveString(Thread* self, ShadowFrame& shadow_frame, uint32_t string_idx)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  Class* java_lang_string_class = String::GetJavaLangString();
  if (UNLIKELY(!java_lang_string_class->IsInitialized())) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> h_class(hs.NewHandle(java_lang_string_class));
    if (UNLIKELY(!class_linker->EnsureInitialized(self, h_class, true, true))) {
      DCHECK(self->IsExceptionPending());
      return nullptr;
    }
  }
  ArtMethod* method = shadow_frame.GetMethod();
  mirror::Class* declaring_class = method->GetDeclaringClass();
  // MethodVerifier refuses methods with string_idx out of bounds.
  DCHECK_LT(string_idx, declaring_class->GetDexCache()->NumStrings());
  mirror::String* s = declaring_class->GetDexCacheStrings()[string_idx].Read();
  if (UNLIKELY(s == nullptr)) {
    StackHandleScope<1> hs(self);
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(declaring_class->GetDexCache()));
    s = Runtime::Current()->GetClassLinker()->ResolveString(*method->GetDexFile(), string_idx,
                                                            dex_cache);
  }
  return s;
}

// Handles div-int, div-int/2addr, div-int/li16 and div-int/lit8 instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoIntDivide(ShadowFrame& shadow_frame, size_t result_reg,
                               int32_t dividend, int32_t divisor)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  constexpr int32_t kMinInt = std::numeric_limits<int32_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinInt && divisor == -1)) {
    shadow_frame.SetVReg(result_reg, kMinInt);
  } else {
    shadow_frame.SetVReg(result_reg, dividend / divisor);
  }
  return true;
}

// Handles rem-int, rem-int/2addr, rem-int/li16 and rem-int/lit8 instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoIntRemainder(ShadowFrame& shadow_frame, size_t result_reg,
                                  int32_t dividend, int32_t divisor)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  constexpr int32_t kMinInt = std::numeric_limits<int32_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinInt && divisor == -1)) {
    shadow_frame.SetVReg(result_reg, 0);
  } else {
    shadow_frame.SetVReg(result_reg, dividend % divisor);
  }
  return true;
}

// Handles div-long and div-long-2addr instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoLongDivide(ShadowFrame& shadow_frame, size_t result_reg,
                                int64_t dividend, int64_t divisor)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  const int64_t kMinLong = std::numeric_limits<int64_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinLong && divisor == -1)) {
    shadow_frame.SetVRegLong(result_reg, kMinLong);
  } else {
    shadow_frame.SetVRegLong(result_reg, dividend / divisor);
  }
  return true;
}

// Handles rem-long and rem-long-2addr instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoLongRemainder(ShadowFrame& shadow_frame, size_t result_reg,
                                   int64_t dividend, int64_t divisor)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  const int64_t kMinLong = std::numeric_limits<int64_t>::min();
  if (UNLIKELY(divisor == 0)) {
    ThrowArithmeticExceptionDivideByZero();
    return false;
  }
  if (UNLIKELY(dividend == kMinLong && divisor == -1)) {
    shadow_frame.SetVRegLong(result_reg, 0);
  } else {
    shadow_frame.SetVRegLong(result_reg, dividend % divisor);
  }
  return true;
}

// Handles filled-new-array and filled-new-array-range instructions.
// Returns true on success, otherwise throws an exception and returns false.
template <bool is_range, bool do_access_check, bool transaction_active>
bool DoFilledNewArray(const Instruction* inst, const ShadowFrame& shadow_frame,
                      Thread* self, JValue* result);

// Handles packed-switch instruction.
// Returns the branch offset to the next instruction to execute.
static inline int32_t DoPackedSwitch(const Instruction* inst, const ShadowFrame& shadow_frame,
                                     uint16_t inst_data)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(inst->Opcode() == Instruction::PACKED_SWITCH);
  const uint16_t* switch_data = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
  int32_t test_val = shadow_frame.GetVReg(inst->VRegA_31t(inst_data));
  DCHECK_EQ(switch_data[0], static_cast<uint16_t>(Instruction::kPackedSwitchSignature));
  uint16_t size = switch_data[1];
  if (size == 0) {
    // Empty packed switch, move forward by 3 (size of PACKED_SWITCH).
    return 3;
  }
  const int32_t* keys = reinterpret_cast<const int32_t*>(&switch_data[2]);
  DCHECK_ALIGNED(keys, 4);
  int32_t first_key = keys[0];
  const int32_t* targets = reinterpret_cast<const int32_t*>(&switch_data[4]);
  DCHECK_ALIGNED(targets, 4);
  int32_t index = test_val - first_key;
  if (index >= 0 && index < size) {
    return targets[index];
  } else {
    // No corresponding value: move forward by 3 (size of PACKED_SWITCH).
    return 3;
  }
}

// Handles sparse-switch instruction.
// Returns the branch offset to the next instruction to execute.
static inline int32_t DoSparseSwitch(const Instruction* inst, const ShadowFrame& shadow_frame,
                                     uint16_t inst_data)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(inst->Opcode() == Instruction::SPARSE_SWITCH);
  const uint16_t* switch_data = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
  int32_t test_val = shadow_frame.GetVReg(inst->VRegA_31t(inst_data));
  DCHECK_EQ(switch_data[0], static_cast<uint16_t>(Instruction::kSparseSwitchSignature));
  uint16_t size = switch_data[1];
  // Return length of SPARSE_SWITCH if size is 0.
  if (size == 0) {
    return 3;
  }
  const int32_t* keys = reinterpret_cast<const int32_t*>(&switch_data[2]);
  DCHECK_ALIGNED(keys, 4);
  const int32_t* entries = keys + size;
  DCHECK_ALIGNED(entries, 4);
  int lo = 0;
  int hi = size - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int32_t foundVal = keys[mid];
    if (test_val < foundVal) {
      hi = mid - 1;
    } else if (test_val > foundVal) {
      lo = mid + 1;
    } else {
      return entries[mid];
    }
  }
  // No corresponding value: move forward by 3 (size of SPARSE_SWITCH).
  return 3;
}

template <bool _do_check>
static inline bool DoBoxLambda(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                               uint16_t inst_data) SHARED_REQUIRES(Locks::mutator_lock_) {
  /*
   * box-lambda vA, vB /// opcode 0xf8, format 22x
   * - vA is the target register where the Object representation of the closure will be stored into
   * - vB is a closure (made by create-lambda)
   *   (also reads vB + 1)
   */
  uint32_t vreg_target_object = inst->VRegA_22x(inst_data);
  uint32_t vreg_source_closure = inst->VRegB_22x();

  lambda::Closure* lambda_closure = ReadLambdaClosureFromVRegsOrThrow(shadow_frame,
                                                                      vreg_source_closure);

  // Failed lambda target runtime check, an exception was raised.
  if (UNLIKELY(lambda_closure == nullptr)) {
    CHECK(self->IsExceptionPending());
    return false;
  }

  mirror::Object* closure_as_object =
      Runtime::Current()->GetLambdaBoxTable()->BoxLambda(lambda_closure);

  // Failed to box the lambda, an exception was raised.
  if (UNLIKELY(closure_as_object == nullptr)) {
    CHECK(self->IsExceptionPending());
    return false;
  }

  shadow_frame.SetVRegReference(vreg_target_object, closure_as_object);
  return true;
}

template <bool _do_check> SHARED_REQUIRES(Locks::mutator_lock_)
static inline bool DoUnboxLambda(Thread* self,
                                 ShadowFrame& shadow_frame,
                                 const Instruction* inst,
                                 uint16_t inst_data) {
  /*
   * unbox-lambda vA, vB, [type id] /// opcode 0xf9, format 22c
   * - vA is the target register where the closure will be written into
   *   (also writes vA + 1)
   * - vB is the Object representation of the closure (made by box-lambda)
   */
  uint32_t vreg_target_closure = inst->VRegA_22c(inst_data);
  uint32_t vreg_source_object = inst->VRegB_22c();

  // Raise NullPointerException if object is null
  mirror::Object* boxed_closure_object = shadow_frame.GetVRegReference(vreg_source_object);
  if (UNLIKELY(boxed_closure_object == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    return false;
  }

  lambda::Closure* unboxed_closure = nullptr;
  // Raise an exception if unboxing fails.
  if (!Runtime::Current()->GetLambdaBoxTable()->UnboxLambda(boxed_closure_object,
                                                            /*out*/&unboxed_closure)) {
    CHECK(self->IsExceptionPending());
    return false;
  }

  DCHECK(unboxed_closure != nullptr);
  WriteLambdaClosureIntoVRegs(/*inout*/shadow_frame, *unboxed_closure, vreg_target_closure);
  return true;
}

uint32_t FindNextInstructionFollowingException(Thread* self, ShadowFrame& shadow_frame,
    uint32_t dex_pc, const instrumentation::Instrumentation* instrumentation)
        SHARED_REQUIRES(Locks::mutator_lock_);

NO_RETURN void UnexpectedOpcode(const Instruction* inst, const ShadowFrame& shadow_frame)
  __attribute__((cold))
  SHARED_REQUIRES(Locks::mutator_lock_);

static inline bool TraceExecutionEnabled() {
  // Return true if you want TraceExecution invocation before each bytecode execution.
  return false;
}

static inline void TraceExecution(const ShadowFrame& shadow_frame, const Instruction* inst,
                                  const uint32_t dex_pc)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  if (TraceExecutionEnabled()) {
#define TRACE_LOG std::cerr
    std::ostringstream oss;
    oss << PrettyMethod(shadow_frame.GetMethod())
        << StringPrintf("\n0x%x: ", dex_pc)
        << inst->DumpString(shadow_frame.GetMethod()->GetDexFile()) << "\n";
    for (uint32_t i = 0; i < shadow_frame.NumberOfVRegs(); ++i) {
      uint32_t raw_value = shadow_frame.GetVReg(i);
      Object* ref_value = shadow_frame.GetVRegReference(i);
      oss << StringPrintf(" vreg%u=0x%08X", i, raw_value);
      if (ref_value != nullptr) {
        if (ref_value->GetClass()->IsStringClass() &&
            ref_value->AsString()->GetValue() != nullptr) {
          oss << "/java.lang.String \"" << ref_value->AsString()->ToModifiedUtf8() << "\"";
        } else {
          oss << "/" << PrettyTypeOf(ref_value);
        }
      }
    }
    TRACE_LOG << oss.str() << "\n";
#undef TRACE_LOG
  }
}

static inline bool IsBackwardBranch(int32_t branch_offset) {
  return branch_offset <= 0;
}

void ArtInterpreterToCompiledCodeBridge(Thread* self,
                                        ArtMethod* caller,
                                        const DexFile::CodeItem* code_item,
                                        ShadowFrame* shadow_frame,
                                        JValue* result);

// Set string value created from StringFactory.newStringFromXXX() into all aliases of
// StringFactory.newEmptyString().
void SetStringInitValueToAllAliases(ShadowFrame* shadow_frame,
                                    uint16_t this_obj_vreg,
                                    JValue result);

// Explicitly instantiate all DoInvoke functions.
#define EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, _is_range, _do_check)                      \
  template SHARED_REQUIRES(Locks::mutator_lock_)                                     \
  bool DoInvoke<_type, _is_range, _do_check>(Thread* self, ShadowFrame& shadow_frame,      \
                                             const Instruction* inst, uint16_t inst_data,  \
                                             JValue* result)

#define EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(_type)       \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, false, false);  \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, false, true);   \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, true, false);   \
  EXPLICIT_DO_INVOKE_TEMPLATE_DECL(_type, true, true);

EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kStatic)      // invoke-static/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kDirect)      // invoke-direct/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kVirtual)     // invoke-virtual/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kSuper)       // invoke-super/range.
EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL(kInterface)   // invoke-interface/range.
#undef EXPLICIT_DO_INVOKE_ALL_TEMPLATE_DECL
#undef EXPLICIT_DO_INVOKE_TEMPLATE_DECL

// Explicitly instantiate all DoInvokeVirtualQuick functions.
#define EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(_is_range)                    \
  template SHARED_REQUIRES(Locks::mutator_lock_)                               \
  bool DoInvokeVirtualQuick<_is_range>(Thread* self, ShadowFrame& shadow_frame,      \
                                       const Instruction* inst, uint16_t inst_data,  \
                                       JValue* result)

EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(false);  // invoke-virtual-quick.
EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(true);   // invoke-virtual-quick-range.
#undef EXPLICIT_INSTANTIATION_DO_INVOKE_VIRTUAL_QUICK

// Explicitly instantiate all DoCreateLambda functions.
#define EXPLICIT_DO_CREATE_LAMBDA_DECL(_do_check)                                                 \
template SHARED_REQUIRES(Locks::mutator_lock_)                                                    \
bool DoCreateLambda<_do_check>(Thread* self,                                                      \
                               const Instruction* inst,                                           \
                               /*inout*/ShadowFrame& shadow_frame,                                \
                               /*inout*/lambda::ClosureBuilder* closure_builder,                  \
                               /*inout*/lambda::Closure* uninitialized_closure);

EXPLICIT_DO_CREATE_LAMBDA_DECL(false);  // create-lambda
EXPLICIT_DO_CREATE_LAMBDA_DECL(true);   // create-lambda
#undef EXPLICIT_DO_CREATE_LAMBDA_DECL

// Explicitly instantiate all DoInvokeLambda functions.
#define EXPLICIT_DO_INVOKE_LAMBDA_DECL(_do_check)                                    \
template SHARED_REQUIRES(Locks::mutator_lock_)                                 \
bool DoInvokeLambda<_do_check>(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst, \
                               uint16_t inst_data, JValue* result);

EXPLICIT_DO_INVOKE_LAMBDA_DECL(false);  // invoke-lambda
EXPLICIT_DO_INVOKE_LAMBDA_DECL(true);   // invoke-lambda
#undef EXPLICIT_DO_INVOKE_LAMBDA_DECL

// Explicitly instantiate all DoBoxLambda functions.
#define EXPLICIT_DO_BOX_LAMBDA_DECL(_do_check)                                                \
template SHARED_REQUIRES(Locks::mutator_lock_)                                          \
bool DoBoxLambda<_do_check>(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst, \
                            uint16_t inst_data);

EXPLICIT_DO_BOX_LAMBDA_DECL(false);  // box-lambda
EXPLICIT_DO_BOX_LAMBDA_DECL(true);   // box-lambda
#undef EXPLICIT_DO_BOX_LAMBDA_DECL

// Explicitly instantiate all DoUnBoxLambda functions.
#define EXPLICIT_DO_UNBOX_LAMBDA_DECL(_do_check)                                                \
template SHARED_REQUIRES(Locks::mutator_lock_)                                            \
bool DoUnboxLambda<_do_check>(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst, \
                              uint16_t inst_data);

EXPLICIT_DO_UNBOX_LAMBDA_DECL(false);  // unbox-lambda
EXPLICIT_DO_UNBOX_LAMBDA_DECL(true);   // unbox-lambda
#undef EXPLICIT_DO_BOX_LAMBDA_DECL

// Explicitly instantiate all DoCaptureVariable functions.
#define EXPLICIT_DO_CAPTURE_VARIABLE_DECL(_do_check)                                    \
template SHARED_REQUIRES(Locks::mutator_lock_)                                          \
bool DoCaptureVariable<_do_check>(Thread* self,                                         \
                                  const Instruction* inst,                              \
                                  ShadowFrame& shadow_frame,                            \
                                  lambda::ClosureBuilder* closure_builder);

EXPLICIT_DO_CAPTURE_VARIABLE_DECL(false);  // capture-variable
EXPLICIT_DO_CAPTURE_VARIABLE_DECL(true);   // capture-variable
#undef EXPLICIT_DO_CREATE_LAMBDA_DECL

// Explicitly instantiate all DoLiberateVariable functions.
#define EXPLICIT_DO_LIBERATE_VARIABLE_DECL(_do_check)                                   \
template SHARED_REQUIRES(Locks::mutator_lock_)                                          \
bool DoLiberateVariable<_do_check>(Thread* self,                                        \
                                   const Instruction* inst,                             \
                                   size_t captured_variable_index,                      \
                                   ShadowFrame& shadow_frame);                          \

EXPLICIT_DO_LIBERATE_VARIABLE_DECL(false);  // liberate-variable
EXPLICIT_DO_LIBERATE_VARIABLE_DECL(true);   // liberate-variable
#undef EXPLICIT_DO_LIBERATE_LAMBDA_DECL
}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_
