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

#include <museum/7.0.0/art/runtime/art_method.h>

#include <museum/7.0.0/art/runtime/arch/context.h>
#include <museum/7.0.0/art/runtime/art_field-inl.h>
#include <museum/7.0.0/art/runtime/art_method-inl.h>
#include <museum/7.0.0/art/runtime/base/stringpiece.h>
#include <museum/7.0.0/art/runtime/class_linker-inl.h>
#include <museum/7.0.0/art/runtime/debugger.h>
#include <museum/7.0.0/art/runtime/dex_file-inl.h>
#include <museum/7.0.0/art/runtime/dex_instruction.h>
#include <museum/7.0.0/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/7.0.0/art/runtime/gc/accounting/card_table-inl.h>
#include <museum/7.0.0/art/runtime/interpreter/interpreter.h>
#include <museum/7.0.0/art/runtime/jit/jit.h>
#include <museum/7.0.0/art/runtime/jit/jit_code_cache.h>
#include <museum/7.0.0/art/runtime/jit/profiling_info.h>
#include <museum/7.0.0/art/runtime/jni_internal.h>
#include <museum/7.0.0/art/runtime/mirror/abstract_method.h>
#include <museum/7.0.0/art/runtime/mirror/class-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object_array-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object-inl.h>
#include <museum/7.0.0/art/runtime/mirror/string.h>
#include <museum/7.0.0/art/runtime/oat_file-inl.h>
#include <museum/7.0.0/art/runtime/scoped_thread_state_change.h>
#include <museum/7.0.0/art/runtime/well_known_classes.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

extern "C" void art_quick_invoke_stub(ArtMethod*, uint32_t*, uint32_t, Thread*, JValue*,
                                      const char*);
extern "C" void art_quick_invoke_static_stub(ArtMethod*, uint32_t*, uint32_t, Thread*, JValue*,
                                             const char*);

ArtMethod* ArtMethod::FromReflectedMethod(const ScopedObjectAccessAlreadyRunnable& soa,
                                          jobject jlr_method) {
  auto* abstract_method = soa.Decode<mirror::AbstractMethod*>(jlr_method);
  DCHECK(abstract_method != nullptr);
  return abstract_method->GetArtMethod();
}

mirror::String* ArtMethod::GetNameAsString(Thread* self) {
  CHECK(!IsProxyMethod());
  StackHandleScope<1> hs(self);
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(GetDexCache()));
  auto* dex_file = dex_cache->GetDexFile();
  uint32_t dex_method_idx = GetDexMethodIndex();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(dex_method_idx);
  return Runtime::Current()->GetClassLinker()->ResolveString(*dex_file, method_id.name_idx_,
                                                             dex_cache);
}

void ArtMethod::ThrowInvocationTimeError() {
  DCHECK(!IsInvokable());
  // NOTE: IsDefaultConflicting must be first since the actual method might or might not be abstract
  //       due to the way we select it.
  if (IsDefaultConflicting()) {
    ThrowIncompatibleClassChangeErrorForMethodConflict(this);
  } else {
    DCHECK(IsAbstract());
    ThrowAbstractMethodError(this);
  }
}

InvokeType ArtMethod::GetInvokeType() {
  // TODO: kSuper?
  if (IsStatic()) {
    return kStatic;
  } else if (GetDeclaringClass()->IsInterface()) {
    return kInterface;
  } else if (IsDirect()) {
    return kDirect;
  } else {
    return kVirtual;
  }
}

size_t ArtMethod::NumArgRegisters(const StringPiece& shorty) {
  CHECK_LE(1U, shorty.length());
  uint32_t num_registers = 0;
  for (size_t i = 1; i < shorty.length(); ++i) {
    char ch = shorty[i];
    if (ch == 'D' || ch == 'J') {
      num_registers += 2;
    } else {
      num_registers += 1;
    }
  }
  return num_registers;
}

bool ArtMethod::HasSameNameAndSignature(ArtMethod* other) {
  ScopedAssertNoThreadSuspension ants(Thread::Current(), "HasSameNameAndSignature");
  const DexFile* dex_file = GetDexFile();
  const DexFile::MethodId& mid = dex_file->GetMethodId(GetDexMethodIndex());
  if (GetDexCache() == other->GetDexCache()) {
    const DexFile::MethodId& mid2 = dex_file->GetMethodId(other->GetDexMethodIndex());
    return mid.name_idx_ == mid2.name_idx_ && mid.proto_idx_ == mid2.proto_idx_;
  }
  const DexFile* dex_file2 = other->GetDexFile();
  const DexFile::MethodId& mid2 = dex_file2->GetMethodId(other->GetDexMethodIndex());
  if (!DexFileStringEquals(dex_file, mid.name_idx_, dex_file2, mid2.name_idx_)) {
    return false;  // Name mismatch.
  }
  return dex_file->GetMethodSignature(mid) == dex_file2->GetMethodSignature(mid2);
}

ArtMethod* ArtMethod::FindOverriddenMethod(size_t pointer_size) {
  if (IsStatic()) {
    return nullptr;
  }
  mirror::Class* declaring_class = GetDeclaringClass();
  mirror::Class* super_class = declaring_class->GetSuperClass();
  uint16_t method_index = GetMethodIndex();
  ArtMethod* result = nullptr;
  // Did this method override a super class method? If so load the result from the super class'
  // vtable
  if (super_class->HasVTable() && method_index < super_class->GetVTableLength()) {
    result = super_class->GetVTableEntry(method_index, pointer_size);
  } else {
    // Method didn't override superclass method so search interfaces
    if (IsProxyMethod()) {
      result = mirror::DexCache::GetElementPtrSize(GetDexCacheResolvedMethods(pointer_size),
                                                   GetDexMethodIndex(),
                                                   pointer_size);
      CHECK_EQ(result,
               Runtime::Current()->GetClassLinker()->FindMethodForProxy(GetDeclaringClass(), this));
    } else {
      mirror::IfTable* iftable = GetDeclaringClass()->GetIfTable();
      for (size_t i = 0; i < iftable->Count() && result == nullptr; i++) {
        mirror::Class* interface = iftable->GetInterface(i);
        for (ArtMethod& interface_method : interface->GetVirtualMethods(pointer_size)) {
          if (HasSameNameAndSignature(interface_method.GetInterfaceMethodIfProxy(pointer_size))) {
            result = &interface_method;
            break;
          }
        }
      }
    }
  }
  DCHECK(result == nullptr ||
         GetInterfaceMethodIfProxy(pointer_size)->HasSameNameAndSignature(
             result->GetInterfaceMethodIfProxy(pointer_size)));
  return result;
}

uint32_t ArtMethod::FindDexMethodIndexInOtherDexFile(const DexFile& other_dexfile,
                                                     uint32_t name_and_signature_idx) {
  const DexFile* dexfile = GetDexFile();
  const uint32_t dex_method_idx = GetDexMethodIndex();
  const DexFile::MethodId& mid = dexfile->GetMethodId(dex_method_idx);
  const DexFile::MethodId& name_and_sig_mid = other_dexfile.GetMethodId(name_and_signature_idx);
  DCHECK_STREQ(dexfile->GetMethodName(mid), other_dexfile.GetMethodName(name_and_sig_mid));
  DCHECK_EQ(dexfile->GetMethodSignature(mid), other_dexfile.GetMethodSignature(name_and_sig_mid));
  if (dexfile == &other_dexfile) {
    return dex_method_idx;
  }
  const char* mid_declaring_class_descriptor = dexfile->StringByTypeIdx(mid.class_idx_);
  const DexFile::TypeId* other_type_id = other_dexfile.FindTypeId(mid_declaring_class_descriptor);
  if (other_type_id != nullptr) {
    const DexFile::MethodId* other_mid = other_dexfile.FindMethodId(
        *other_type_id, other_dexfile.GetStringId(name_and_sig_mid.name_idx_),
        other_dexfile.GetProtoId(name_and_sig_mid.proto_idx_));
    if (other_mid != nullptr) {
      return other_dexfile.GetIndexForMethodId(*other_mid);
    }
  }
  return DexFile::kDexNoIndex;
}

uint32_t ArtMethod::FindCatchBlock(Handle<mirror::Class> exception_type,
                                   uint32_t dex_pc, bool* has_no_move_exception) {
  const DexFile::CodeItem* code_item = GetCodeItem();
  // Set aside the exception while we resolve its type.
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::Throwable> exception(hs.NewHandle(self->GetException()));
  self->ClearException();
  // Default to handler not found.
  uint32_t found_dex_pc = DexFile::kDexNoIndex;
  // Iterate over the catch handlers associated with dex_pc.
  size_t pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
  for (CatchHandlerIterator it(*code_item, dex_pc); it.HasNext(); it.Next()) {
    uint16_t iter_type_idx = it.GetHandlerTypeIndex();
    // Catch all case
    if (iter_type_idx == DexFile::kDexNoIndex16) {
      found_dex_pc = it.GetHandlerAddress();
      break;
    }
    // Does this catch exception type apply?
    mirror::Class* iter_exception_type = GetClassFromTypeIndex(iter_type_idx,
                                                               true /* resolve */,
                                                               pointer_size);
    if (UNLIKELY(iter_exception_type == nullptr)) {
      // Now have a NoClassDefFoundError as exception. Ignore in case the exception class was
      // removed by a pro-guard like tool.
      // Note: this is not RI behavior. RI would have failed when loading the class.
      self->ClearException();
      // Delete any long jump context as this routine is called during a stack walk which will
      // release its in use context at the end.
      delete self->GetLongJumpContext();
      LOG(WARNING) << "Unresolved exception class when finding catch block: "
        << DescriptorToDot(GetTypeDescriptorFromTypeIdx(iter_type_idx));
    } else if (iter_exception_type->IsAssignableFrom(exception_type.Get())) {
      found_dex_pc = it.GetHandlerAddress();
      break;
    }
  }
  if (found_dex_pc != DexFile::kDexNoIndex) {
    const Instruction* first_catch_instr =
        Instruction::At(&code_item->insns_[found_dex_pc]);
    *has_no_move_exception = (first_catch_instr->Opcode() != Instruction::MOVE_EXCEPTION);
  }
  // Put the exception back.
  if (exception.Get() != nullptr) {
    self->SetException(exception.Get());
  }
  return found_dex_pc;
}

void ArtMethod::Invoke(Thread* self, uint32_t* args, uint32_t args_size, JValue* result,
                       const char* shorty) {
  if (UNLIKELY(__builtin_frame_address(0) < self->GetStackEnd())) {
    ThrowStackOverflowError(self);
    return;
  }

  if (kIsDebugBuild) {
    self->AssertThreadSuspensionIsAllowable();
    CHECK_EQ(kRunnable, self->GetState());
    CHECK_STREQ(GetInterfaceMethodIfProxy(sizeof(void*))->GetShorty(), shorty);
  }

  // Push a transition back into managed code onto the linked list in thread.
  ManagedStack fragment;
  self->PushManagedStackFragment(&fragment);

  Runtime* runtime = Runtime::Current();
  // Call the invoke stub, passing everything as arguments.
  // If the runtime is not yet started or it is required by the debugger, then perform the
  // Invocation by the interpreter, explicitly forcing interpretation over JIT to prevent
  // cycling around the various JIT/Interpreter methods that handle method invocation.
  if (UNLIKELY(!runtime->IsStarted() || Dbg::IsForcedInterpreterNeededForCalling(self, this))) {
    if (IsStatic()) {
      art::interpreter::EnterInterpreterFromInvoke(
          self, this, nullptr, args, result, /*stay_in_interpreter*/ true);
    } else {
      mirror::Object* receiver =
          reinterpret_cast<StackReference<mirror::Object>*>(&args[0])->AsMirrorPtr();
      art::interpreter::EnterInterpreterFromInvoke(
          self, this, receiver, args + 1, result, /*stay_in_interpreter*/ true);
    }
  } else {
    DCHECK_EQ(runtime->GetClassLinker()->GetImagePointerSize(), sizeof(void*));

    constexpr bool kLogInvocationStartAndReturn = false;
    bool have_quick_code = GetEntryPointFromQuickCompiledCode() != nullptr;
    if (LIKELY(have_quick_code)) {
      if (kLogInvocationStartAndReturn) {
        LOG(INFO) << StringPrintf(
            "Invoking '%s' quick code=%p static=%d", PrettyMethod(this).c_str(),
            GetEntryPointFromQuickCompiledCode(), static_cast<int>(IsStatic() ? 1 : 0));
      }

      // Ensure that we won't be accidentally calling quick compiled code when -Xint.
      if (kIsDebugBuild && runtime->GetInstrumentation()->IsForcedInterpretOnly()) {
        CHECK(!runtime->UseJitCompilation());
        const void* oat_quick_code = runtime->GetClassLinker()->GetOatMethodQuickCodeFor(this);
        CHECK(oat_quick_code == nullptr || oat_quick_code != GetEntryPointFromQuickCompiledCode())
            << "Don't call compiled code when -Xint " << PrettyMethod(this);
      }

      if (!IsStatic()) {
        (*art_quick_invoke_stub)(this, args, args_size, self, result, shorty);
      } else {
        (*art_quick_invoke_static_stub)(this, args, args_size, self, result, shorty);
      }
      if (UNLIKELY(self->GetException() == Thread::GetDeoptimizationException())) {
        // Unusual case where we were running generated code and an
        // exception was thrown to force the activations to be removed from the
        // stack. Continue execution in the interpreter.
        self->DeoptimizeWithDeoptimizationException(result);
      }
      if (kLogInvocationStartAndReturn) {
        LOG(INFO) << StringPrintf("Returned '%s' quick code=%p", PrettyMethod(this).c_str(),
                                  GetEntryPointFromQuickCompiledCode());
      }
    } else {
      LOG(INFO) << "Not invoking '" << PrettyMethod(this) << "' code=null";
      if (result != nullptr) {
        result->SetJ(0);
      }
    }
  }

  // Pop transition.
  self->PopManagedStackFragment(fragment);
}

void ArtMethod::RegisterNative(const void* native_method, bool is_fast) {
  CHECK(IsNative()) << PrettyMethod(this);
  CHECK(!IsFastNative()) << PrettyMethod(this);
  CHECK(native_method != nullptr) << PrettyMethod(this);
  if (is_fast) {
    SetAccessFlags(GetAccessFlags() | kAccFastNative);
  }
  SetEntryPointFromJni(native_method);
}

void ArtMethod::UnregisterNative() {
  CHECK(IsNative() && !IsFastNative()) << PrettyMethod(this);
  // restore stub to lookup native pointer via dlsym
  RegisterNative(GetJniDlsymLookupStub(), false);
}

bool ArtMethod::IsOverridableByDefaultMethod() {
  return GetDeclaringClass()->IsInterface();
}

bool ArtMethod::EqualParameters(Handle<mirror::ObjectArray<mirror::Class>> params) {
  auto* dex_cache = GetDexCache();
  auto* dex_file = dex_cache->GetDexFile();
  const auto& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const auto& proto_id = dex_file->GetMethodPrototype(method_id);
  const DexFile::TypeList* proto_params = dex_file->GetProtoParameters(proto_id);
  auto count = proto_params != nullptr ? proto_params->Size() : 0u;
  auto param_len = params.Get() != nullptr ? params->GetLength() : 0u;
  if (param_len != count) {
    return false;
  }
  auto* cl = Runtime::Current()->GetClassLinker();
  for (size_t i = 0; i < count; ++i) {
    auto type_idx = proto_params->GetTypeItem(i).type_idx_;
    auto* type = cl->ResolveType(type_idx, this);
    if (type == nullptr) {
      Thread::Current()->AssertPendingException();
      return false;
    }
    if (type != params->GetWithoutChecks(i)) {
      return false;
    }
  }
  return true;
}

const uint8_t* ArtMethod::GetQuickenedInfo() {
  bool found = false;
  OatFile::OatMethod oat_method =
      Runtime::Current()->GetClassLinker()->FindOatMethodFor(this, &found);
  if (!found || (oat_method.GetQuickCode() != nullptr)) {
    return nullptr;
  }
  return oat_method.GetVmapTable();
}

const OatQuickMethodHeader* ArtMethod::GetOatQuickMethodHeader(uintptr_t pc) {
  // Our callers should make sure they don't pass the instrumentation exit pc,
  // as this method does not look at the side instrumentation stack.
  DCHECK_NE(pc, reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc()));

  if (IsRuntimeMethod()) {
    return nullptr;
  }

  Runtime* runtime = Runtime::Current();
  const void* existing_entry_point = GetEntryPointFromQuickCompiledCode();
  //CHECK(existing_entry_point != nullptr) << PrettyMethod(this) << "@" << this;
  ClassLinker* class_linker = runtime->GetClassLinker();

  if (class_linker->IsQuickGenericJniStub(existing_entry_point)) {
    // The generic JNI does not have any method header.
    return nullptr;
  }

  if (existing_entry_point == GetQuickProxyInvokeHandler()) {
    DCHECK(IsProxyMethod() && !IsConstructor());
    // The proxy entry point does not have any method header.
    return nullptr;
  }

  // Check whether the current entry point contains this pc.
  if (!class_linker->IsQuickResolutionStub(existing_entry_point) &&
      !class_linker->IsQuickToInterpreterBridge(existing_entry_point)) {
    OatQuickMethodHeader* method_header =
        OatQuickMethodHeader::FromEntryPoint(existing_entry_point);

    if (method_header->Contains(pc)) {
      return method_header;
    }
  }

  // Check whether the pc is in the JIT code cache.
  // FB
  //jit::Jit* jit = Runtime::Current()->GetJit();
  //if (jit != nullptr) {
  //  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  //  OatQuickMethodHeader* method_header = code_cache->LookupMethodHeader(pc, this);
  //  if (method_header != nullptr) {
  //    DCHECK(method_header->Contains(pc));
  //    return method_header;
  //  } else {
  //    DCHECK(!code_cache->ContainsPc(reinterpret_cast<const void*>(pc)))
  //        << PrettyMethod(this)
  //        << ", pc=" << std::hex << pc
  //        << ", entry_point=" << std::hex << reinterpret_cast<uintptr_t>(existing_entry_point)
  //        << ", copy=" << std::boolalpha << IsCopied()
  //        << ", proxy=" << std::boolalpha << IsProxyMethod();
  //  }
  //}
  // FB END

  // The code has to be in an oat file.
  bool found;
  OatFile::OatMethod oat_method = class_linker->FindOatMethodFor(this, &found);
  if (!found) {
    if (class_linker->IsQuickResolutionStub(existing_entry_point)) {
      // We are running the generic jni stub, but the entry point of the method has not
      // been updated yet.
      DCHECK_EQ(pc, 0u) << "Should be a downcall";
      DCHECK(IsNative());
      return nullptr;
    }
    if (existing_entry_point == GetQuickInstrumentationEntryPoint()) {
      // We are running the generic jni stub, but the method is being instrumented.
      DCHECK_EQ(pc, 0u) << "Should be a downcall";
      DCHECK(IsNative());
      return nullptr;
    }
    // Only for unit tests.
    // TODO(ngeoffray): Update these tests to pass the right pc?
    return OatQuickMethodHeader::FromEntryPoint(existing_entry_point);
  }
  const void* oat_entry_point = oat_method.GetQuickCode();
  if (oat_entry_point == nullptr || class_linker->IsQuickGenericJniStub(oat_entry_point)) {
    DCHECK(IsNative()) << PrettyMethod(this);
    return nullptr;
  }

  OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromEntryPoint(oat_entry_point);
  if (pc == 0) {
    // This is a downcall, it can only happen for a native method.
    DCHECK(IsNative());
    return method_header;
  }

  DCHECK(method_header->Contains(pc))
      << PrettyMethod(this)
      << std::hex << pc << " " << oat_entry_point
      << " " << (uintptr_t)(method_header->code_ + method_header->code_size_);
  return method_header;
}

bool ArtMethod::HasAnyCompiledCode() {
  // Check whether the JIT has compiled it.
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr && jit->GetCodeCache()->ContainsMethod(this)) {
    return true;
  }

  // Check whether we have AOT code.
  return Runtime::Current()->GetClassLinker()->GetOatMethodQuickCodeFor(this) != nullptr;
}

void ArtMethod::CopyFrom(ArtMethod* src, size_t image_pointer_size) {
  memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(src),
         Size(image_pointer_size));
  declaring_class_ = GcRoot<mirror::Class>(const_cast<ArtMethod*>(src)->GetDeclaringClass());

  // If the entry point of the method we are copying from is from JIT code, we just
  // put the entry point of the new method to interpreter. We could set the entry point
  // to the JIT code, but this would require taking the JIT code cache lock to notify
  // it, which we do not want at this level.
  Runtime* runtime = Runtime::Current();
  if (runtime->UseJitCompilation()) {
    if (runtime->GetJit()->GetCodeCache()->ContainsPc(GetEntryPointFromQuickCompiledCode())) {
      SetEntryPointFromQuickCompiledCodePtrSize(GetQuickToInterpreterBridge(), image_pointer_size);
    }
  }
  // Clear the profiling info for the same reasons as the JIT code.
  if (!src->IsNative()) {
    SetProfilingInfoPtrSize(nullptr, image_pointer_size);
  }
  // Clear hotness to let the JIT properly decide when to compile this method.
  hotness_count_ = 0;
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
