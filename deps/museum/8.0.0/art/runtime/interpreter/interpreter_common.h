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
#include "interpreter_intrinsics.h"

#include <math.h>

#include <iostream>
#include <sstream>
#include <atomic>

#include "android-base/stringprintf.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/logging.h"
#include "base/macros.h"
#include "class_linker-inl.h"
#include "common_dex_operations.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "handle_scope-inl.h"
#include "jit/jit.h"
#include "mirror/call_site.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/method.h"
#include "mirror/method_handles_lookup.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "obj_ptr.h"
#include "stack.h"
#include "thread.h"
#include "unstarted_runtime.h"
#include "well_known_classes.h"

namespace art {
namespace interpreter {

void ThrowNullPointerExceptionFromInterpreter()
    REQUIRES_SHARED(Locks::mutator_lock_);

template <bool kMonitorCounting>
static inline void DoMonitorEnter(Thread* self, ShadowFrame* frame, ObjPtr<mirror::Object> ref)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_ref(hs.NewHandle(ref));
  h_ref->MonitorEnter(self);
  if (kMonitorCounting && frame->GetMethod()->MustCountLocks()) {
    frame->GetLockCountData().AddMonitor(self, h_ref.Get());
  }
}

template <bool kMonitorCounting>
static inline void DoMonitorExit(Thread* self, ShadowFrame* frame, ObjPtr<mirror::Object> ref)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_ref(hs.NewHandle(ref));
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
    REQUIRES_SHARED(Locks::mutator_lock_);

void AbortTransactionV(Thread* self, const char* fmt, va_list args)
    REQUIRES_SHARED(Locks::mutator_lock_);

void RecordArrayElementsInTransaction(ObjPtr<mirror::Array> array, int32_t count)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Invokes the given method. This is part of the invocation support and is used by DoInvoke,
// DoFastInvoke and DoInvokeVirtualQuick functions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range, bool do_assignability_check>
bool DoCall(ArtMethod* called_method, Thread* self, ShadowFrame& shadow_frame,
            const Instruction* inst, uint16_t inst_data, JValue* result);

// Handles streamlined non-range invoke static, direct and virtual instructions originating in
// mterp. Access checks and instrumentation other than jit profiling are not supported, but does
// support interpreter intrinsics if applicable.
// Returns true on success, otherwise throws an exception and returns false.
template<InvokeType type>
static inline bool DoFastInvoke(Thread* self,
                                ShadowFrame& shadow_frame,
                                const Instruction* inst,
                                uint16_t inst_data,
                                JValue* result) {
  const uint32_t method_idx = inst->VRegB_35c();
  const uint32_t vregC = inst->VRegC_35c();
  ObjPtr<mirror::Object> receiver = (type == kStatic)
      ? nullptr
      : shadow_frame.GetVRegReference(vregC);
  ArtMethod* sf_method = shadow_frame.GetMethod();
  ArtMethod* const called_method = FindMethodFromCode<type, false>(
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
      if (type == kVirtual) {
        jit->InvokeVirtualOrInterface(receiver, sf_method, shadow_frame.GetDexPC(), called_method);
      }
      jit->AddSamples(self, sf_method, 1, /*with_backedges*/false);
    }
    if (called_method->IsIntrinsic()) {
      if (MterpHandleIntrinsic(&shadow_frame, called_method, inst, inst_data,
                               shadow_frame.GetResultRegister())) {
        return !self->IsExceptionPending();
      }
    }
    return DoCall<false, false>(called_method, self, shadow_frame, inst, inst_data, result);
  }
}

// Handles all invoke-XXX/range instructions except for invoke-polymorphic[/range].
// Returns true on success, otherwise throws an exception and returns false.
template<InvokeType type, bool is_range, bool do_access_check>
static inline bool DoInvoke(Thread* self,
                            ShadowFrame& shadow_frame,
                            const Instruction* inst,
                            uint16_t inst_data,
                            JValue* result) {
  const uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  ObjPtr<mirror::Object> receiver = (type == kStatic) ? nullptr : shadow_frame.GetVRegReference(vregC);
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
        jit->InvokeVirtualOrInterface(receiver, sf_method, shadow_frame.GetDexPC(), called_method);
      }
      jit->AddSamples(self, sf_method, 1, /*with_backedges*/false);
    }
    // TODO: Remove the InvokeVirtualOrInterface instrumentation, as it was only used by the JIT.
    if (type == kVirtual || type == kInterface) {
      instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
      if (UNLIKELY(instrumentation->HasInvokeVirtualOrInterfaceListeners())) {
        instrumentation->InvokeVirtualOrInterface(
            self, receiver.Ptr(), sf_method, shadow_frame.GetDexPC(), called_method);
      }
    }
    return DoCall<is_range, do_access_check>(called_method, self, shadow_frame, inst, inst_data,
                                             result);
  }
}

// Performs a signature polymorphic invoke (invoke-polymorphic/invoke-polymorphic-range).
template<bool is_range>
bool DoInvokePolymorphic(Thread* self,
                         ShadowFrame& shadow_frame,
                         const Instruction* inst,
                         uint16_t inst_data,
                         JValue* result);

// Performs a custom invoke (invoke-custom/invoke-custom-range).
template<bool is_range>
bool DoInvokeCustom(Thread* self,
                    ShadowFrame& shadow_frame,
                    const Instruction* inst,
                    uint16_t inst_data,
                    JValue* result);

// Handles invoke-virtual-quick and invoke-virtual-quick-range instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<bool is_range>
static inline bool DoInvokeVirtualQuick(Thread* self, ShadowFrame& shadow_frame,
                                        const Instruction* inst, uint16_t inst_data,
                                        JValue* result) {
  const uint32_t vregC = (is_range) ? inst->VRegC_3rc() : inst->VRegC_35c();
  ObjPtr<mirror::Object> const receiver = shadow_frame.GetVRegReference(vregC);
  if (UNLIKELY(receiver == nullptr)) {
    // We lost the reference to the method index so we cannot get a more
    // precised exception message.
    ThrowNullPointerExceptionFromDexPC();
    return false;
  }
  const uint32_t vtable_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
  // Debug code for b/31357497. To be removed.
  if (kUseReadBarrier) {
    CHECK(receiver->GetClass() != nullptr)
        << "Null class found in object " << receiver << " in region type "
        << Runtime::Current()->GetHeap()->ConcurrentCopyingCollector()->
            RegionSpace()->GetRegionType(receiver.Ptr());
  }
  CHECK(receiver->GetClass()->ShouldHaveEmbeddedVTable());
  ArtMethod* const called_method = receiver->GetClass()->GetEmbeddedVTableEntry(
      vtable_idx, kRuntimePointerSize);
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
          receiver, shadow_frame.GetMethod(), shadow_frame.GetDexPC(), called_method);
      jit->AddSamples(self, shadow_frame.GetMethod(), 1, /*with_backedges*/false);
    }
    instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
    // TODO: Remove the InvokeVirtualOrInterface instrumentation, as it was only used by the JIT.
    if (UNLIKELY(instrumentation->HasInvokeVirtualOrInterfaceListeners())) {
      instrumentation->InvokeVirtualOrInterface(
          self, receiver.Ptr(), shadow_frame.GetMethod(), shadow_frame.GetDexPC(), called_method);
    }
    // No need to check since we've been quickened.
    return DoCall<is_range, false>(called_method, self, shadow_frame, inst, inst_data, result);
  }
}

// Handles iget-XXX and sget-XXX instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check>
bool DoFieldGet(Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,
                uint16_t inst_data) REQUIRES_SHARED(Locks::mutator_lock_);

// Handles iget-quick, iget-wide-quick and iget-object-quick instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<Primitive::Type field_type>
bool DoIGetQuick(ShadowFrame& shadow_frame, const Instruction* inst, uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Handles iput-XXX and sput-XXX instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<FindFieldType find_type, Primitive::Type field_type, bool do_access_check,
         bool transaction_active>
bool DoFieldPut(Thread* self, const ShadowFrame& shadow_frame, const Instruction* inst,
                uint16_t inst_data) REQUIRES_SHARED(Locks::mutator_lock_);

// Handles iput-quick, iput-wide-quick and iput-object-quick instructions.
// Returns true on success, otherwise throws an exception and returns false.
template<Primitive::Type field_type, bool transaction_active>
bool DoIPutQuick(const ShadowFrame& shadow_frame, const Instruction* inst, uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_);


// Handles string resolution for const-string and const-string-jumbo instructions. Also ensures the
// java.lang.String class is initialized.
static inline ObjPtr<mirror::String> ResolveString(Thread* self,
                                                   ShadowFrame& shadow_frame,
                                                   dex::StringIndex string_idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> java_lang_string_class = mirror::String::GetJavaLangString();
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
  ObjPtr<mirror::String> string_ptr = method->GetDexCache()->GetResolvedString(string_idx);
  if (UNLIKELY(string_ptr == nullptr)) {
    StackHandleScope<1> hs(self);
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(method->GetDexCache()));
    string_ptr = Runtime::Current()->GetClassLinker()->ResolveString(*dex_cache->GetDexFile(),
                                                                     string_idx,
                                                                     dex_cache);
  }
  return string_ptr;
}

// Handles div-int, div-int/2addr, div-int/li16 and div-int/lit8 instructions.
// Returns true on success, otherwise throws a java.lang.ArithmeticException and return false.
static inline bool DoIntDivide(ShadowFrame& shadow_frame, size_t result_reg,
                               int32_t dividend, int32_t divisor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
static inline bool DoLongDivide(ShadowFrame& shadow_frame,
                                size_t result_reg,
                                int64_t dividend,
                                int64_t divisor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
static inline bool DoLongRemainder(ShadowFrame& shadow_frame,
                                   size_t result_reg,
                                   int64_t dividend,
                                   int64_t divisor)
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
    REQUIRES_SHARED(Locks::mutator_lock_) {
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

uint32_t FindNextInstructionFollowingException(Thread* self, ShadowFrame& shadow_frame,
    uint32_t dex_pc, const instrumentation::Instrumentation* instrumentation)
        REQUIRES_SHARED(Locks::mutator_lock_);

NO_RETURN void UnexpectedOpcode(const Instruction* inst, const ShadowFrame& shadow_frame)
  __attribute__((cold))
  REQUIRES_SHARED(Locks::mutator_lock_);

// Set true if you want TraceExecution invocation before each bytecode execution.
constexpr bool kTraceExecutionEnabled = false;

static inline void TraceExecution(const ShadowFrame& shadow_frame, const Instruction* inst,
                                  const uint32_t dex_pc)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (kTraceExecutionEnabled) {
#define TRACE_LOG std::cerr
    std::ostringstream oss;
    oss << shadow_frame.GetMethod()->PrettyMethod()
        << android::base::StringPrintf("\n0x%x: ", dex_pc)
        << inst->DumpString(shadow_frame.GetMethod()->GetDexFile()) << "\n";
    for (uint32_t i = 0; i < shadow_frame.NumberOfVRegs(); ++i) {
      uint32_t raw_value = shadow_frame.GetVReg(i);
      ObjPtr<mirror::Object> ref_value = shadow_frame.GetVRegReference(i);
      oss << android::base::StringPrintf(" vreg%u=0x%08X", i, raw_value);
      if (ref_value != nullptr) {
        if (ref_value->GetClass()->IsStringClass() &&
            !ref_value->AsString()->IsValueNull()) {
          oss << "/java.lang.String \"" << ref_value->AsString()->ToModifiedUtf8() << "\"";
        } else {
          oss << "/" << ref_value->PrettyTypeOf();
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

// Assign register 'src_reg' from shadow_frame to register 'dest_reg' into new_shadow_frame.
static inline void AssignRegister(ShadowFrame* new_shadow_frame, const ShadowFrame& shadow_frame,
                                  size_t dest_reg, size_t src_reg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Uint required, so that sign extension does not make this wrong on 64b systems
  uint32_t src_value = shadow_frame.GetVReg(src_reg);
  ObjPtr<mirror::Object> o = shadow_frame.GetVRegReference<kVerifyNone>(src_reg);

  // If both register locations contains the same value, the register probably holds a reference.
  // Note: As an optimization, non-moving collectors leave a stale reference value
  // in the references array even after the original vreg was overwritten to a non-reference.
  if (src_value == reinterpret_cast<uintptr_t>(o.Ptr())) {
    new_shadow_frame->SetVRegReference(dest_reg, o.Ptr());
  } else {
    new_shadow_frame->SetVReg(dest_reg, src_value);
  }
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
  template REQUIRES_SHARED(Locks::mutator_lock_)                                           \
  bool DoInvoke<_type, _is_range, _do_check>(Thread* self,                                 \
                                             ShadowFrame& shadow_frame,                    \
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

// Explicitly instantiate all DoFastInvoke functions.
#define EXPLICIT_DO_FAST_INVOKE_TEMPLATE_DECL(_type)                     \
  template REQUIRES_SHARED(Locks::mutator_lock_)                         \
  bool DoFastInvoke<_type>(Thread* self,                                 \
                           ShadowFrame& shadow_frame,                    \
                           const Instruction* inst, uint16_t inst_data,  \
                           JValue* result)

EXPLICIT_DO_FAST_INVOKE_TEMPLATE_DECL(kStatic);     // invoke-static
EXPLICIT_DO_FAST_INVOKE_TEMPLATE_DECL(kDirect);     // invoke-direct
EXPLICIT_DO_FAST_INVOKE_TEMPLATE_DECL(kVirtual);    // invoke-virtual
#undef EXPLICIT_DO_FAST_INVOKE_TEMPLATE_DECL

// Explicitly instantiate all DoInvokeVirtualQuick functions.
#define EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(_is_range)                    \
  template REQUIRES_SHARED(Locks::mutator_lock_)                               \
  bool DoInvokeVirtualQuick<_is_range>(Thread* self, ShadowFrame& shadow_frame,      \
                                       const Instruction* inst, uint16_t inst_data,  \
                                       JValue* result)

EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(false);  // invoke-virtual-quick.
EXPLICIT_DO_INVOKE_VIRTUAL_QUICK_TEMPLATE_DECL(true);   // invoke-virtual-quick-range.
#undef EXPLICIT_INSTANTIATION_DO_INVOKE_VIRTUAL_QUICK

}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_COMMON_H_
