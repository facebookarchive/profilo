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

#include <museum/6.0.1/art/runtime/instrumentation.h>

#include <museum/6.0.1/external/libcxx/sstream>

#include <museum/6.0.1/art/runtime/arch/context.h>
#include <museum/6.0.1/art/runtime/art_method-inl.h>
#include <museum/6.0.1/art/runtime/atomic.h>
#include <museum/6.0.1/art/runtime/class_linker.h>
#include <museum/6.0.1/art/runtime/debugger.h>
#include <museum/6.0.1/art/runtime/dex_file-inl.h>
#include <museum/6.0.1/art/runtime/entrypoints/quick/quick_entrypoints.h>
#include <museum/6.0.1/art/runtime/entrypoints/quick/quick_alloc_entrypoints.h>
#include <museum/6.0.1/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/6.0.1/art/runtime/gc_root-inl.h>
#include <museum/6.0.1/art/runtime/interpreter/interpreter.h>
#include <museum/6.0.1/art/runtime/jit/jit.h>
#include <museum/6.0.1/art/runtime/jit/jit_code_cache.h>
#include <museum/6.0.1/art/runtime/mirror/class-inl.h>
#include <museum/6.0.1/art/runtime/mirror/dex_cache.h>
#include <museum/6.0.1/art/runtime/mirror/object_array-inl.h>
#include <museum/6.0.1/art/runtime/mirror/object-inl.h>
#include <museum/6.0.1/art/runtime/nth_caller_visitor.h>
#include <museum/6.0.1/art/runtime/thread.h>
#include <museum/6.0.1/art/runtime/thread_list.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace instrumentation {

constexpr bool kVerboseInstrumentation = false;

// Instrumentation works on non-inlined frames by updating returned PCs
// of compiled frames.
static constexpr StackVisitor::StackWalkKind kInstrumentationStackWalk =
    StackVisitor::StackWalkKind::kSkipInlinedFrames;

static bool InstallStubsClassVisitor(mirror::Class* klass, void* arg)
    EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
  instrumentation->InstallStubsForClass(klass);
  return true;  // we visit all classes.
}

Instrumentation::Instrumentation()
    : instrumentation_stubs_installed_(false), entry_exit_stubs_installed_(false),
      interpreter_stubs_installed_(false),
      interpret_only_(false), forced_interpret_only_(false),
      have_method_entry_listeners_(false), have_method_exit_listeners_(false),
      have_method_unwind_listeners_(false), have_dex_pc_listeners_(false),
      have_field_read_listeners_(false), have_field_write_listeners_(false),
      have_exception_caught_listeners_(false), have_backward_branch_listeners_(false),
      deoptimized_methods_lock_("deoptimized methods lock"),
      deoptimization_enabled_(false),
      interpreter_handler_table_(kMainHandlerTable),
      quick_alloc_entry_points_instrumentation_counter_(0) {
}

void Instrumentation::InstallStubsForClass(mirror::Class* klass) {
  if (klass->IsErroneous()) {
    // We can't execute code in a erroneous class: do nothing.
  } else if (!klass->IsResolved()) {
    // We need the class to be resolved to install/uninstall stubs. Otherwise its methods
    // could not be initialized or linked with regards to class inheritance.
  } else {
    for (size_t i = 0, e = klass->NumDirectMethods(); i < e; i++) {
      InstallStubsForMethod(klass->GetDirectMethod(i, sizeof(void*)));
    }
    for (size_t i = 0, e = klass->NumVirtualMethods(); i < e; i++) {
      InstallStubsForMethod(klass->GetVirtualMethod(i, sizeof(void*)));
    }
  }
}

static void UpdateEntrypoints(ArtMethod* method, const void* quick_code)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Runtime* const runtime = Runtime::Current();
  jit::Jit* jit = runtime->GetJit();
  if (jit != nullptr) {
    const void* old_code_ptr = method->GetEntryPointFromQuickCompiledCode();
    jit::JitCodeCache* code_cache = jit->GetCodeCache();
    if (code_cache->ContainsCodePtr(old_code_ptr)) {
      // Save the old compiled code since we need it to implement ClassLinker::GetQuickOatCodeFor.
      code_cache->SaveCompiledCode(method, old_code_ptr);
    }
  }
  method->SetEntryPointFromQuickCompiledCode(quick_code);
  if (!method->IsResolutionMethod()) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    if (class_linker->IsQuickToInterpreterBridge(quick_code) ||
        (class_linker->IsQuickResolutionStub(quick_code) &&
         Runtime::Current()->GetInstrumentation()->IsForcedInterpretOnly() &&
         !method->IsNative() && !method->IsProxyMethod())) {
      DCHECK(!method->IsNative()) << PrettyMethod(method);
      DCHECK(!method->IsProxyMethod()) << PrettyMethod(method);
      method->SetEntryPointFromInterpreter(art::artInterpreterToInterpreterBridge);
    } else {
      method->SetEntryPointFromInterpreter(art::artInterpreterToCompiledCodeBridge);
    }
  }
}

void Instrumentation::InstallStubsForMethod(ArtMethod* method) {
  if (method->IsAbstract() || method->IsProxyMethod()) {
    // Do not change stubs for these methods.
    return;
  }
  // Don't stub Proxy.<init>. Note that the Proxy class itself is not a proxy class.
  if (method->IsConstructor() &&
      method->GetDeclaringClass()->DescriptorEquals("Ljava/lang/reflect/Proxy;")) {
    return;
  }
  const void* new_quick_code;
  bool uninstall = !entry_exit_stubs_installed_ && !interpreter_stubs_installed_;
  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  bool is_class_initialized = method->GetDeclaringClass()->IsInitialized();
  if (uninstall) {
    if ((forced_interpret_only_ || IsDeoptimized(method)) && !method->IsNative()) {
      new_quick_code = GetQuickToInterpreterBridge();
    } else if (is_class_initialized || !method->IsStatic() || method->IsConstructor()) {
      new_quick_code = class_linker->GetQuickOatCodeFor(method);
    } else {
      new_quick_code = GetQuickResolutionStub();
    }
  } else {  // !uninstall
    if ((interpreter_stubs_installed_ || forced_interpret_only_ || IsDeoptimized(method)) &&
        !method->IsNative()) {
      new_quick_code = GetQuickToInterpreterBridge();
    } else {
      // Do not overwrite resolution trampoline. When the trampoline initializes the method's
      // class, all its static methods code will be set to the instrumentation entry point.
      // For more details, see ClassLinker::FixupStaticTrampolines.
      if (is_class_initialized || !method->IsStatic() || method->IsConstructor()) {
        if (entry_exit_stubs_installed_) {
          new_quick_code = GetQuickInstrumentationEntryPoint();
        } else {
          new_quick_code = class_linker->GetQuickOatCodeFor(method);
        }
      } else {
        new_quick_code = GetQuickResolutionStub();
      }
    }
  }
  UpdateEntrypoints(method, new_quick_code);
}

// Places the instrumentation exit pc as the return PC for every quick frame. This also allows
// deoptimization of quick frames to interpreter frames.
// Since we may already have done this previously, we need to push new instrumentation frame before
// existing instrumentation frames.
static void InstrumentationInstallStack(Thread* thread, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  struct InstallStackVisitor FINAL : public StackVisitor {
    InstallStackVisitor(Thread* thread_in, Context* context, uintptr_t instrumentation_exit_pc)
        : StackVisitor(thread_in, context, kInstrumentationStackWalk),
          instrumentation_stack_(thread_in->GetInstrumentationStack()),
          instrumentation_exit_pc_(instrumentation_exit_pc),
          reached_existing_instrumentation_frames_(false), instrumentation_stack_depth_(0),
          last_return_pc_(0) {
    }

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      ArtMethod* m = GetMethod();
      if (m == nullptr) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping upcall. Frame " << GetFrameId();
        }
        last_return_pc_ = 0;
        return true;  // Ignore upcalls.
      }
      if (GetCurrentQuickFrame() == nullptr) {
        bool interpreter_frame = true;
        InstrumentationStackFrame instrumentation_frame(GetThisObject(), m, 0, GetFrameId(),
                                                        interpreter_frame);
        if (kVerboseInstrumentation) {
          LOG(INFO) << "Pushing shadow frame " << instrumentation_frame.Dump();
        }
        shadow_stack_.push_back(instrumentation_frame);
        return true;  // Continue.
      }
      uintptr_t return_pc = GetReturnPc();
      if (m->IsRuntimeMethod()) {
        if (return_pc == instrumentation_exit_pc_) {
          if (kVerboseInstrumentation) {
            LOG(INFO) << "  Handling quick to interpreter transition. Frame " << GetFrameId();
          }
          CHECK_LT(instrumentation_stack_depth_, instrumentation_stack_->size());
          const InstrumentationStackFrame& frame =
              instrumentation_stack_->at(instrumentation_stack_depth_);
          CHECK(frame.interpreter_entry_);
          // This is an interpreter frame so method enter event must have been reported. However we
          // need to push a DEX pc into the dex_pcs_ list to match size of instrumentation stack.
          // Since we won't report method entry here, we can safely push any DEX pc.
          dex_pcs_.push_back(0);
          last_return_pc_ = frame.return_pc_;
          ++instrumentation_stack_depth_;
          return true;
        } else {
          if (kVerboseInstrumentation) {
            LOG(INFO) << "  Skipping runtime method. Frame " << GetFrameId();
          }
          last_return_pc_ = GetReturnPc();
          return true;  // Ignore unresolved methods since they will be instrumented after resolution.
        }
      }
      if (kVerboseInstrumentation) {
        LOG(INFO) << "  Installing exit stub in " << DescribeLocation();
      }
      if (return_pc == instrumentation_exit_pc_) {
        // We've reached a frame which has already been installed with instrumentation exit stub.
        // We should have already installed instrumentation on previous frames.
        reached_existing_instrumentation_frames_ = true;

        CHECK_LT(instrumentation_stack_depth_, instrumentation_stack_->size());
        const InstrumentationStackFrame& frame =
            instrumentation_stack_->at(instrumentation_stack_depth_);
        CHECK_EQ(m, frame.method_) << "Expected " << PrettyMethod(m)
                                   << ", Found " << PrettyMethod(frame.method_);
        return_pc = frame.return_pc_;
        if (kVerboseInstrumentation) {
          LOG(INFO) << "Ignoring already instrumented " << frame.Dump();
        }
      } else {
        CHECK_NE(return_pc, 0U);
        CHECK(!reached_existing_instrumentation_frames_);
        InstrumentationStackFrame instrumentation_frame(GetThisObject(), m, return_pc, GetFrameId(),
                                                        false);
        if (kVerboseInstrumentation) {
          LOG(INFO) << "Pushing frame " << instrumentation_frame.Dump();
        }

        // Insert frame at the right position so we do not corrupt the instrumentation stack.
        // Instrumentation stack frames are in descending frame id order.
        auto it = instrumentation_stack_->begin();
        for (auto end = instrumentation_stack_->end(); it != end; ++it) {
          const InstrumentationStackFrame& current = *it;
          if (instrumentation_frame.frame_id_ >= current.frame_id_) {
            break;
          }
        }
        instrumentation_stack_->insert(it, instrumentation_frame);
        SetReturnPc(instrumentation_exit_pc_);
      }
      dex_pcs_.push_back(m->ToDexPc(last_return_pc_));
      last_return_pc_ = return_pc;
      ++instrumentation_stack_depth_;
      return true;  // Continue.
    }
    std::deque<InstrumentationStackFrame>* const instrumentation_stack_;
    std::vector<InstrumentationStackFrame> shadow_stack_;
    std::vector<uint32_t> dex_pcs_;
    const uintptr_t instrumentation_exit_pc_;
    bool reached_existing_instrumentation_frames_;
    size_t instrumentation_stack_depth_;
    uintptr_t last_return_pc_;
  };
  if (kVerboseInstrumentation) {
    std::string thread_name;
    thread->GetThreadName(thread_name);
    LOG(INFO) << "Installing exit stubs in " << thread_name;
  }

  Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
  std::unique_ptr<Context> context(Context::Create());
  uintptr_t instrumentation_exit_pc = reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc());
  InstallStackVisitor visitor(thread, context.get(), instrumentation_exit_pc);
  visitor.WalkStack(true);
  CHECK_EQ(visitor.dex_pcs_.size(), thread->GetInstrumentationStack()->size());

  if (instrumentation->ShouldNotifyMethodEnterExitEvents()) {
    // Create method enter events for all methods currently on the thread's stack. We only do this
    // if no debugger is attached to prevent from posting events twice.
    auto ssi = visitor.shadow_stack_.rbegin();
    for (auto isi = thread->GetInstrumentationStack()->rbegin(),
        end = thread->GetInstrumentationStack()->rend(); isi != end; ++isi) {
      while (ssi != visitor.shadow_stack_.rend() && (*ssi).frame_id_ < (*isi).frame_id_) {
        instrumentation->MethodEnterEvent(thread, (*ssi).this_object_, (*ssi).method_, 0);
        ++ssi;
      }
      uint32_t dex_pc = visitor.dex_pcs_.back();
      visitor.dex_pcs_.pop_back();
      if (!isi->interpreter_entry_) {
        instrumentation->MethodEnterEvent(thread, (*isi).this_object_, (*isi).method_, dex_pc);
      }
    }
  }
  thread->VerifyStack();
}

// Removes the instrumentation exit pc as the return PC for every quick frame.
static void InstrumentationRestoreStack(Thread* thread, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  struct RestoreStackVisitor FINAL : public StackVisitor {
    RestoreStackVisitor(Thread* thread_in, uintptr_t instrumentation_exit_pc,
                        Instrumentation* instrumentation)
        : StackVisitor(thread_in, nullptr, kInstrumentationStackWalk),
          thread_(thread_in),
          instrumentation_exit_pc_(instrumentation_exit_pc),
          instrumentation_(instrumentation),
          instrumentation_stack_(thread_in->GetInstrumentationStack()),
          frames_removed_(0) {}

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      if (instrumentation_stack_->size() == 0) {
        return false;  // Stop.
      }
      ArtMethod* m = GetMethod();
      if (GetCurrentQuickFrame() == nullptr) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Ignoring a shadow frame. Frame " << GetFrameId()
              << " Method=" << PrettyMethod(m);
        }
        return true;  // Ignore shadow frames.
      }
      if (m == nullptr) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping upcall. Frame " << GetFrameId();
        }
        return true;  // Ignore upcalls.
      }
      bool removed_stub = false;
      // TODO: make this search more efficient?
      const size_t frameId = GetFrameId();
      for (const InstrumentationStackFrame& instrumentation_frame : *instrumentation_stack_) {
        if (instrumentation_frame.frame_id_ == frameId) {
          if (kVerboseInstrumentation) {
            LOG(INFO) << "  Removing exit stub in " << DescribeLocation();
          }
          if (instrumentation_frame.interpreter_entry_) {
            CHECK(m == Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs));
          } else {
            CHECK(m == instrumentation_frame.method_) << PrettyMethod(m);
          }
          SetReturnPc(instrumentation_frame.return_pc_);
          if (instrumentation_->ShouldNotifyMethodEnterExitEvents()) {
            // Create the method exit events. As the methods didn't really exit the result is 0.
            // We only do this if no debugger is attached to prevent from posting events twice.
            instrumentation_->MethodExitEvent(thread_, instrumentation_frame.this_object_, m,
                                              GetDexPc(), JValue());
          }
          frames_removed_++;
          removed_stub = true;
          break;
        }
      }
      if (!removed_stub) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  No exit stub in " << DescribeLocation();
        }
      }
      return true;  // Continue.
    }
    Thread* const thread_;
    const uintptr_t instrumentation_exit_pc_;
    Instrumentation* const instrumentation_;
    std::deque<instrumentation::InstrumentationStackFrame>* const instrumentation_stack_;
    size_t frames_removed_;
  };
  if (kVerboseInstrumentation) {
    std::string thread_name;
    thread->GetThreadName(thread_name);
    LOG(INFO) << "Removing exit stubs in " << thread_name;
  }
  std::deque<instrumentation::InstrumentationStackFrame>* stack = thread->GetInstrumentationStack();
  if (stack->size() > 0) {
    Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
    uintptr_t instrumentation_exit_pc =
        reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc());
    RestoreStackVisitor visitor(thread, instrumentation_exit_pc, instrumentation);
    visitor.WalkStack(true);
    CHECK_EQ(visitor.frames_removed_, stack->size());
    while (stack->size() > 0) {
      stack->pop_front();
    }
  }
}

static bool HasEvent(Instrumentation::InstrumentationEvent expected, uint32_t events) {
  return (events & expected) != 0;
}

void Instrumentation::AddListener(InstrumentationListener* listener, uint32_t events) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  if (HasEvent(kMethodEntered, events)) {
    method_entry_listeners_.push_back(listener);
    have_method_entry_listeners_ = true;
  }
  if (HasEvent(kMethodExited, events)) {
    method_exit_listeners_.push_back(listener);
    have_method_exit_listeners_ = true;
  }
  if (HasEvent(kMethodUnwind, events)) {
    method_unwind_listeners_.push_back(listener);
    have_method_unwind_listeners_ = true;
  }
  if (HasEvent(kBackwardBranch, events)) {
    backward_branch_listeners_.push_back(listener);
    have_backward_branch_listeners_ = true;
  }
  if (HasEvent(kDexPcMoved, events)) {
    std::list<InstrumentationListener*>* modified;
    if (have_dex_pc_listeners_) {
      modified = new std::list<InstrumentationListener*>(*dex_pc_listeners_.get());
    } else {
      modified = new std::list<InstrumentationListener*>();
    }
    modified->push_back(listener);
    dex_pc_listeners_.reset(modified);
    have_dex_pc_listeners_ = true;
  }
  if (HasEvent(kFieldRead, events)) {
    std::list<InstrumentationListener*>* modified;
    if (have_field_read_listeners_) {
      modified = new std::list<InstrumentationListener*>(*field_read_listeners_.get());
    } else {
      modified = new std::list<InstrumentationListener*>();
    }
    modified->push_back(listener);
    field_read_listeners_.reset(modified);
    have_field_read_listeners_ = true;
  }
  if (HasEvent(kFieldWritten, events)) {
    std::list<InstrumentationListener*>* modified;
    if (have_field_write_listeners_) {
      modified = new std::list<InstrumentationListener*>(*field_write_listeners_.get());
    } else {
      modified = new std::list<InstrumentationListener*>();
    }
    modified->push_back(listener);
    field_write_listeners_.reset(modified);
    have_field_write_listeners_ = true;
  }
  if (HasEvent(kExceptionCaught, events)) {
    std::list<InstrumentationListener*>* modified;
    if (have_exception_caught_listeners_) {
      modified = new std::list<InstrumentationListener*>(*exception_caught_listeners_.get());
    } else {
      modified = new std::list<InstrumentationListener*>();
    }
    modified->push_back(listener);
    exception_caught_listeners_.reset(modified);
    have_exception_caught_listeners_ = true;
  }
  UpdateInterpreterHandlerTable();
}

void Instrumentation::RemoveListener(InstrumentationListener* listener, uint32_t events) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());

  if (HasEvent(kMethodEntered, events) && have_method_entry_listeners_) {
    method_entry_listeners_.remove(listener);
    have_method_entry_listeners_ = !method_entry_listeners_.empty();
  }
  if (HasEvent(kMethodExited, events) && have_method_exit_listeners_) {
    method_exit_listeners_.remove(listener);
    have_method_exit_listeners_ = !method_exit_listeners_.empty();
  }
  if (HasEvent(kMethodUnwind, events) && have_method_unwind_listeners_) {
      method_unwind_listeners_.remove(listener);
      have_method_unwind_listeners_ = !method_unwind_listeners_.empty();
  }
  if (HasEvent(kBackwardBranch, events) && have_backward_branch_listeners_) {
      backward_branch_listeners_.remove(listener);
      have_backward_branch_listeners_ = !backward_branch_listeners_.empty();
    }
  if (HasEvent(kDexPcMoved, events) && have_dex_pc_listeners_) {
    std::list<InstrumentationListener*>* modified =
        new std::list<InstrumentationListener*>(*dex_pc_listeners_.get());
    modified->remove(listener);
    have_dex_pc_listeners_ = !modified->empty();
    if (have_dex_pc_listeners_) {
      dex_pc_listeners_.reset(modified);
    } else {
      dex_pc_listeners_.reset();
      delete modified;
    }
  }
  if (HasEvent(kFieldRead, events) && have_field_read_listeners_) {
    std::list<InstrumentationListener*>* modified =
        new std::list<InstrumentationListener*>(*field_read_listeners_.get());
    modified->remove(listener);
    have_field_read_listeners_ = !modified->empty();
    if (have_field_read_listeners_) {
      field_read_listeners_.reset(modified);
    } else {
      field_read_listeners_.reset();
      delete modified;
    }
  }
  if (HasEvent(kFieldWritten, events) && have_field_write_listeners_) {
    std::list<InstrumentationListener*>* modified =
        new std::list<InstrumentationListener*>(*field_write_listeners_.get());
    modified->remove(listener);
    have_field_write_listeners_ = !modified->empty();
    if (have_field_write_listeners_) {
      field_write_listeners_.reset(modified);
    } else {
      field_write_listeners_.reset();
      delete modified;
    }
  }
  if (HasEvent(kExceptionCaught, events) && have_exception_caught_listeners_) {
    std::list<InstrumentationListener*>* modified =
        new std::list<InstrumentationListener*>(*exception_caught_listeners_.get());
    modified->remove(listener);
    have_exception_caught_listeners_ = !modified->empty();
    if (have_exception_caught_listeners_) {
      exception_caught_listeners_.reset(modified);
    } else {
      exception_caught_listeners_.reset();
      delete modified;
    }
  }
  UpdateInterpreterHandlerTable();
}

Instrumentation::InstrumentationLevel Instrumentation::GetCurrentInstrumentationLevel() const {
  if (interpreter_stubs_installed_) {
    return InstrumentationLevel::kInstrumentWithInterpreter;
  } else if (entry_exit_stubs_installed_) {
    return InstrumentationLevel::kInstrumentWithInstrumentationStubs;
  } else {
    return InstrumentationLevel::kInstrumentNothing;
  }
}

void Instrumentation::ConfigureStubs(const char* key, InstrumentationLevel desired_level) {
  // Store the instrumentation level for this key or remove it.
  if (desired_level == InstrumentationLevel::kInstrumentNothing) {
    // The client no longer needs instrumentation.
    requested_instrumentation_levels_.erase(key);
  } else {
    // The client needs instrumentation.
    requested_instrumentation_levels_.Overwrite(key, desired_level);
  }

  // Look for the highest required instrumentation level.
  InstrumentationLevel requested_level = InstrumentationLevel::kInstrumentNothing;
  for (const auto& v : requested_instrumentation_levels_) {
    requested_level = std::max(requested_level, v.second);
  }

  interpret_only_ = (requested_level == InstrumentationLevel::kInstrumentWithInterpreter) ||
                    forced_interpret_only_;

  InstrumentationLevel current_level = GetCurrentInstrumentationLevel();
  if (requested_level == current_level) {
    // We're already set.
    return;
  }
  Thread* const self = Thread::Current();
  Runtime* runtime = Runtime::Current();
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  Locks::thread_list_lock_->AssertNotHeld(self);
  if (requested_level > InstrumentationLevel::kInstrumentNothing) {
    if (requested_level == InstrumentationLevel::kInstrumentWithInterpreter) {
      interpreter_stubs_installed_ = true;
      entry_exit_stubs_installed_ = true;
    } else {
      CHECK_EQ(requested_level, InstrumentationLevel::kInstrumentWithInstrumentationStubs);
      entry_exit_stubs_installed_ = true;
      interpreter_stubs_installed_ = false;
    }
    runtime->GetClassLinker()->VisitClasses(InstallStubsClassVisitor, this);
    instrumentation_stubs_installed_ = true;
    MutexLock mu(self, *Locks::thread_list_lock_);
    runtime->GetThreadList()->ForEach(InstrumentationInstallStack, this);
  } else {
    interpreter_stubs_installed_ = false;
    entry_exit_stubs_installed_ = false;
    runtime->GetClassLinker()->VisitClasses(InstallStubsClassVisitor, this);
    // Restore stack only if there is no method currently deoptimized.
    bool empty;
    {
      ReaderMutexLock mu(self, deoptimized_methods_lock_);
      empty = IsDeoptimizedMethodsEmpty();  // Avoid lock violation.
    }
    if (empty) {
      instrumentation_stubs_installed_ = false;
      MutexLock mu(self, *Locks::thread_list_lock_);
      Runtime::Current()->GetThreadList()->ForEach(InstrumentationRestoreStack, this);
    }
  }
}

static void ResetQuickAllocEntryPointsForThread(Thread* thread, void* arg ATTRIBUTE_UNUSED) {
  thread->ResetQuickAllocEntryPointsForThread();
}

void Instrumentation::SetEntrypointsInstrumented(bool instrumented) {
  Thread* self = Thread::Current();
  Runtime* runtime = Runtime::Current();
  ThreadList* tl = runtime->GetThreadList();
  Locks::mutator_lock_->AssertNotHeld(self);
  Locks::instrument_entrypoints_lock_->AssertHeld(self);
  if (runtime->IsStarted()) {
    tl->SuspendAll(__FUNCTION__);
  }
  {
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    SetQuickAllocEntryPointsInstrumented(instrumented);
    ResetQuickAllocEntryPoints();
  }
  if (runtime->IsStarted()) {
    tl->ResumeAll();
  }
}

void Instrumentation::InstrumentQuickAllocEntryPoints() {
  MutexLock mu(Thread::Current(), *Locks::instrument_entrypoints_lock_);
  InstrumentQuickAllocEntryPointsLocked();
}

void Instrumentation::UninstrumentQuickAllocEntryPoints() {
  MutexLock mu(Thread::Current(), *Locks::instrument_entrypoints_lock_);
  UninstrumentQuickAllocEntryPointsLocked();
}

void Instrumentation::InstrumentQuickAllocEntryPointsLocked() {
  Locks::instrument_entrypoints_lock_->AssertHeld(Thread::Current());
  if (quick_alloc_entry_points_instrumentation_counter_ == 0) {
    SetEntrypointsInstrumented(true);
  }
  ++quick_alloc_entry_points_instrumentation_counter_;
}

void Instrumentation::UninstrumentQuickAllocEntryPointsLocked() {
  Locks::instrument_entrypoints_lock_->AssertHeld(Thread::Current());
  CHECK_GT(quick_alloc_entry_points_instrumentation_counter_, 0U);
  --quick_alloc_entry_points_instrumentation_counter_;
  if (quick_alloc_entry_points_instrumentation_counter_ == 0) {
    SetEntrypointsInstrumented(false);
  }
}

void Instrumentation::ResetQuickAllocEntryPoints() {
  Runtime* runtime = Runtime::Current();
  if (runtime->IsStarted()) {
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    runtime->GetThreadList()->ForEach(ResetQuickAllocEntryPointsForThread, nullptr);
  }
}

void Instrumentation::UpdateMethodsCode(ArtMethod* method, const void* quick_code) {
  DCHECK(method->GetDeclaringClass()->IsResolved());
  const void* new_quick_code;
  if (LIKELY(!instrumentation_stubs_installed_)) {
    new_quick_code = quick_code;
  } else {
    if ((interpreter_stubs_installed_ || IsDeoptimized(method)) && !method->IsNative()) {
      new_quick_code = GetQuickToInterpreterBridge();
    } else {
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      if (class_linker->IsQuickResolutionStub(quick_code) ||
          class_linker->IsQuickToInterpreterBridge(quick_code)) {
        new_quick_code = quick_code;
      } else if (entry_exit_stubs_installed_) {
        new_quick_code = GetQuickInstrumentationEntryPoint();
      } else {
        new_quick_code = quick_code;
      }
    }
  }
  UpdateEntrypoints(method, new_quick_code);
}

bool Instrumentation::AddDeoptimizedMethod(ArtMethod* method) {
  if (IsDeoptimizedMethod(method)) {
    // Already in the map. Return.
    return false;
  }
  // Not found. Add it.
  deoptimized_methods_.insert(method);
  return true;
}

bool Instrumentation::IsDeoptimizedMethod(ArtMethod* method) {
  return deoptimized_methods_.find(method) != deoptimized_methods_.end();
}

ArtMethod* Instrumentation::BeginDeoptimizedMethod() {
  if (deoptimized_methods_.empty()) {
    // Empty.
    return nullptr;
  }
  return *deoptimized_methods_.begin();
}

bool Instrumentation::RemoveDeoptimizedMethod(ArtMethod* method) {
  auto it = deoptimized_methods_.find(method);
  if (it == deoptimized_methods_.end()) {
    return false;
  }
  deoptimized_methods_.erase(it);
  return true;
}

bool Instrumentation::IsDeoptimizedMethodsEmpty() const {
  return deoptimized_methods_.empty();
}

void Instrumentation::Deoptimize(ArtMethod* method) {
  CHECK(!method->IsNative());
  CHECK(!method->IsProxyMethod());
  CHECK(!method->IsAbstract());

  Thread* self = Thread::Current();
  {
    WriterMutexLock mu(self, deoptimized_methods_lock_);
    bool has_not_been_deoptimized = AddDeoptimizedMethod(method);
    CHECK(has_not_been_deoptimized) << "Method " << PrettyMethod(method)
        << " is already deoptimized";
  }
  if (!interpreter_stubs_installed_) {
    UpdateEntrypoints(method, GetQuickInstrumentationEntryPoint());

    // Install instrumentation exit stub and instrumentation frames. We may already have installed
    // these previously so it will only cover the newly created frames.
    instrumentation_stubs_installed_ = true;
    MutexLock mu(self, *Locks::thread_list_lock_);
    Runtime::Current()->GetThreadList()->ForEach(InstrumentationInstallStack, this);
  }
}

void Instrumentation::Undeoptimize(ArtMethod* method) {
  CHECK(!method->IsNative());
  CHECK(!method->IsProxyMethod());
  CHECK(!method->IsAbstract());

  Thread* self = Thread::Current();
  bool empty;
  {
    WriterMutexLock mu(self, deoptimized_methods_lock_);
    bool found_and_erased = RemoveDeoptimizedMethod(method);
    CHECK(found_and_erased) << "Method " << PrettyMethod(method)
        << " is not deoptimized";
    empty = IsDeoptimizedMethodsEmpty();
  }

  // Restore code and possibly stack only if we did not deoptimize everything.
  if (!interpreter_stubs_installed_) {
    // Restore its code or resolution trampoline.
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    if (method->IsStatic() && !method->IsConstructor() &&
        !method->GetDeclaringClass()->IsInitialized()) {
      UpdateEntrypoints(method, GetQuickResolutionStub());
    } else {
      const void* quick_code = class_linker->GetQuickOatCodeFor(method);
      UpdateEntrypoints(method, quick_code);
    }

    // If there is no deoptimized method left, we can restore the stack of each thread.
    if (empty) {
      MutexLock mu(self, *Locks::thread_list_lock_);
      Runtime::Current()->GetThreadList()->ForEach(InstrumentationRestoreStack, this);
      instrumentation_stubs_installed_ = false;
    }
  }
}

bool Instrumentation::IsDeoptimized(ArtMethod* method) {
  DCHECK(method != nullptr);
  ReaderMutexLock mu(Thread::Current(), deoptimized_methods_lock_);
  return IsDeoptimizedMethod(method);
}

void Instrumentation::EnableDeoptimization() {
  ReaderMutexLock mu(Thread::Current(), deoptimized_methods_lock_);
  CHECK(IsDeoptimizedMethodsEmpty());
  CHECK_EQ(deoptimization_enabled_, false);
  deoptimization_enabled_ = true;
}

void Instrumentation::DisableDeoptimization(const char* key) {
  CHECK_EQ(deoptimization_enabled_, true);
  // If we deoptimized everything, undo it.
  if (interpreter_stubs_installed_) {
    UndeoptimizeEverything(key);
  }
  // Undeoptimized selected methods.
  while (true) {
    ArtMethod* method;
    {
      ReaderMutexLock mu(Thread::Current(), deoptimized_methods_lock_);
      if (IsDeoptimizedMethodsEmpty()) {
        break;
      }
      method = BeginDeoptimizedMethod();
      CHECK(method != nullptr);
    }
    Undeoptimize(method);
  }
  deoptimization_enabled_ = false;
}

// Indicates if instrumentation should notify method enter/exit events to the listeners.
bool Instrumentation::ShouldNotifyMethodEnterExitEvents() const {
  if (!HasMethodEntryListeners() && !HasMethodExitListeners()) {
    return false;
  }
  return !deoptimization_enabled_ && !interpreter_stubs_installed_;
}

void Instrumentation::DeoptimizeEverything(const char* key) {
  CHECK(deoptimization_enabled_);
  ConfigureStubs(key, InstrumentationLevel::kInstrumentWithInterpreter);
}

void Instrumentation::UndeoptimizeEverything(const char* key) {
  CHECK(interpreter_stubs_installed_);
  CHECK(deoptimization_enabled_);
  ConfigureStubs(key, InstrumentationLevel::kInstrumentNothing);
}

void Instrumentation::EnableMethodTracing(const char* key, bool needs_interpreter) {
  InstrumentationLevel level;
  if (needs_interpreter) {
    level = InstrumentationLevel::kInstrumentWithInterpreter;
  } else {
    level = InstrumentationLevel::kInstrumentWithInstrumentationStubs;
  }
  ConfigureStubs(key, level);
}

void Instrumentation::DisableMethodTracing(const char* key) {
  ConfigureStubs(key, InstrumentationLevel::kInstrumentNothing);
}

const void* Instrumentation::GetQuickCodeFor(ArtMethod* method, size_t pointer_size) const {
  Runtime* runtime = Runtime::Current();
  if (LIKELY(!instrumentation_stubs_installed_)) {
    const void* code = method->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
    DCHECK(code != nullptr);
    ClassLinker* class_linker = runtime->GetClassLinker();
    if (LIKELY(!class_linker->IsQuickResolutionStub(code) &&
               !class_linker->IsQuickToInterpreterBridge(code)) &&
               !class_linker->IsQuickResolutionStub(code) &&
               !class_linker->IsQuickToInterpreterBridge(code)) {
      return code;
    }
  }
  // FB - DO NOT SUPPORT INSTRUMENTATION HOOKS UNWINDING
  return nullptr; //return runtime->GetClassLinker()->GetQuickOatCodeFor(method);
}

void Instrumentation::MethodEnterEventImpl(Thread* thread, mirror::Object* this_object,
                                           ArtMethod* method,
                                           uint32_t dex_pc) const {
  auto it = method_entry_listeners_.begin();
  bool is_end = (it == method_entry_listeners_.end());
  // Implemented this way to prevent problems caused by modification of the list while iterating.
  while (!is_end) {
    InstrumentationListener* cur = *it;
    ++it;
    is_end = (it == method_entry_listeners_.end());
    cur->MethodEntered(thread, this_object, method, dex_pc);
  }
}

void Instrumentation::MethodExitEventImpl(Thread* thread, mirror::Object* this_object,
                                          ArtMethod* method,
                                          uint32_t dex_pc, const JValue& return_value) const {
  auto it = method_exit_listeners_.begin();
  bool is_end = (it == method_exit_listeners_.end());
  // Implemented this way to prevent problems caused by modification of the list while iterating.
  while (!is_end) {
    InstrumentationListener* cur = *it;
    ++it;
    is_end = (it == method_exit_listeners_.end());
    cur->MethodExited(thread, this_object, method, dex_pc, return_value);
  }
}

void Instrumentation::MethodUnwindEvent(Thread* thread, mirror::Object* this_object,
                                        ArtMethod* method,
                                        uint32_t dex_pc) const {
  if (HasMethodUnwindListeners()) {
    for (InstrumentationListener* listener : method_unwind_listeners_) {
      listener->MethodUnwind(thread, this_object, method, dex_pc);
    }
  }
}

void Instrumentation::DexPcMovedEventImpl(Thread* thread, mirror::Object* this_object,
                                          ArtMethod* method,
                                          uint32_t dex_pc) const {
  std::shared_ptr<std::list<InstrumentationListener*>> original(dex_pc_listeners_);
  for (InstrumentationListener* listener : *original.get()) {
    listener->DexPcMoved(thread, this_object, method, dex_pc);
  }
}

void Instrumentation::BackwardBranchImpl(Thread* thread, ArtMethod* method,
                                         int32_t offset) const {
  for (InstrumentationListener* listener : backward_branch_listeners_) {
    listener->BackwardBranch(thread, method, offset);
  }
}

void Instrumentation::FieldReadEventImpl(Thread* thread, mirror::Object* this_object,
                                         ArtMethod* method, uint32_t dex_pc,
                                         ArtField* field) const {
  std::shared_ptr<std::list<InstrumentationListener*>> original(field_read_listeners_);
  for (InstrumentationListener* listener : *original.get()) {
    listener->FieldRead(thread, this_object, method, dex_pc, field);
  }
}

void Instrumentation::FieldWriteEventImpl(Thread* thread, mirror::Object* this_object,
                                         ArtMethod* method, uint32_t dex_pc,
                                         ArtField* field, const JValue& field_value) const {
  std::shared_ptr<std::list<InstrumentationListener*>> original(field_write_listeners_);
  for (InstrumentationListener* listener : *original.get()) {
    listener->FieldWritten(thread, this_object, method, dex_pc, field, field_value);
  }
}

void Instrumentation::ExceptionCaughtEvent(Thread* thread,
                                           mirror::Throwable* exception_object) const {
  if (HasExceptionCaughtListeners()) {
    DCHECK_EQ(thread->GetException(), exception_object);
    thread->ClearException();
    std::shared_ptr<std::list<InstrumentationListener*>> original(exception_caught_listeners_);
    for (InstrumentationListener* listener : *original.get()) {
      listener->ExceptionCaught(thread, exception_object);
    }
    thread->SetException(exception_object);
  }
}

static void CheckStackDepth(Thread* self, const InstrumentationStackFrame& instrumentation_frame,
                            int delta)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  size_t frame_id = StackVisitor::ComputeNumFrames(self, kInstrumentationStackWalk) + delta;
  if (frame_id != instrumentation_frame.frame_id_) {
    LOG(ERROR) << "Expected frame_id=" << frame_id << " but found "
        << instrumentation_frame.frame_id_;
    StackVisitor::DescribeStack(self);
    CHECK_EQ(frame_id, instrumentation_frame.frame_id_);
  }
}

void Instrumentation::PushInstrumentationStackFrame(Thread* self, mirror::Object* this_object,
                                                    ArtMethod* method,
                                                    uintptr_t lr, bool interpreter_entry) {
  // We have a callee-save frame meaning this value is guaranteed to never be 0.
  size_t frame_id = StackVisitor::ComputeNumFrames(self, kInstrumentationStackWalk);
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  if (kVerboseInstrumentation) {
    LOG(INFO) << "Entering " << PrettyMethod(method) << " from PC " << reinterpret_cast<void*>(lr);
  }
  instrumentation::InstrumentationStackFrame instrumentation_frame(this_object, method, lr,
                                                                   frame_id, interpreter_entry);
  stack->push_front(instrumentation_frame);

  if (!interpreter_entry) {
    MethodEnterEvent(self, this_object, method, 0);
  }
}

TwoWordReturn Instrumentation::PopInstrumentationStackFrame(Thread* self, uintptr_t* return_pc,
                                                            uint64_t gpr_result,
                                                            uint64_t fpr_result) {
  // Do the pop.
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  CHECK_GT(stack->size(), 0U);
  InstrumentationStackFrame instrumentation_frame = stack->front();
  stack->pop_front();

  // Set return PC and check the sanity of the stack.
  *return_pc = instrumentation_frame.return_pc_;
  CheckStackDepth(self, instrumentation_frame, 0);
  self->VerifyStack();

  ArtMethod* method = instrumentation_frame.method_;
  uint32_t length;
  char return_shorty = method->GetShorty(&length)[0];
  JValue return_value;
  if (return_shorty == 'V') {
    return_value.SetJ(0);
  } else if (return_shorty == 'F' || return_shorty == 'D') {
    return_value.SetJ(fpr_result);
  } else {
    return_value.SetJ(gpr_result);
  }
  // TODO: improve the dex pc information here, requires knowledge of current PC as opposed to
  //       return_pc.
  uint32_t dex_pc = DexFile::kDexNoIndex;
  mirror::Object* this_object = instrumentation_frame.this_object_;
  if (!instrumentation_frame.interpreter_entry_) {
    MethodExitEvent(self, this_object, instrumentation_frame.method_, dex_pc, return_value);
  }

  // Deoptimize if the caller needs to continue execution in the interpreter. Do nothing if we get
  // back to an upcall.
  NthCallerVisitor visitor(self, 1, true);
  visitor.WalkStack(true);
  bool deoptimize = (visitor.caller != nullptr) &&
                    (interpreter_stubs_installed_ || IsDeoptimized(visitor.caller) ||
                    Dbg::IsForcedInterpreterNeededForUpcall(self, visitor.caller));
  if (deoptimize) {
    if (kVerboseInstrumentation) {
      LOG(INFO) << StringPrintf("Deoptimizing %s by returning from %s with result %#" PRIx64 " in ",
                                PrettyMethod(visitor.caller).c_str(),
                                PrettyMethod(method).c_str(),
                                return_value.GetJ()) << *self;
    }
    self->SetDeoptimizationReturnValue(return_value, return_shorty == 'L');
    return GetTwoWordSuccessValue(*return_pc,
                                  reinterpret_cast<uintptr_t>(GetQuickDeoptimizationEntryPoint()));
  } else {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Returning from " << PrettyMethod(method)
                << " to PC " << reinterpret_cast<void*>(*return_pc);
    }
    return GetTwoWordSuccessValue(0, *return_pc);
  }
}

void Instrumentation::PopMethodForUnwind(Thread* self, bool is_deoptimization) const {
  // Do the pop.
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  CHECK_GT(stack->size(), 0U);
  InstrumentationStackFrame instrumentation_frame = stack->front();
  // TODO: bring back CheckStackDepth(self, instrumentation_frame, 2);
  stack->pop_front();

  ArtMethod* method = instrumentation_frame.method_;
  if (is_deoptimization) {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Popping for deoptimization " << PrettyMethod(method);
    }
  } else {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Popping for unwind " << PrettyMethod(method);
    }

    // Notify listeners of method unwind.
    // TODO: improve the dex pc information here, requires knowledge of current PC as opposed to
    //       return_pc.
    uint32_t dex_pc = DexFile::kDexNoIndex;
    MethodUnwindEvent(self, instrumentation_frame.this_object_, method, dex_pc);
  }
}

std::string InstrumentationStackFrame::Dump() const {
  std::ostringstream os;
  os << "Frame " << frame_id_ << " " << PrettyMethod(method_) << ":"
      << reinterpret_cast<void*>(return_pc_) << " this=" << reinterpret_cast<void*>(this_object_);
  return os.str();
}

}  // namespace instrumentation
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
