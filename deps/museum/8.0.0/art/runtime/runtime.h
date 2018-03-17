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

#ifndef ART_RUNTIME_RUNTIME_H_
#define ART_RUNTIME_RUNTIME_H_

#include <jni.h>
#include <stdio.h>

#include <iosfwd>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "arch/instruction_set.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "deoptimization_kind.h"
#include "dex_file_types.h"
#include "experimental_flags.h"
#include "gc_root.h"
#include "instrumentation.h"
#include "jobject_comparator.h"
#include "method_reference.h"
#include "obj_ptr.h"
#include "object_callbacks.h"
#include "offsets.h"
#include "process_state.h"
#include "quick/quick_method_frame_info.h"
#include "runtime_stats.h"

namespace art {

namespace gc {
  class AbstractSystemWeakHolder;
  class Heap;
  namespace collector {
    class GarbageCollector;
  }  // namespace collector
}  // namespace gc

namespace jit {
  class Jit;
  class JitOptions;
}  // namespace jit

namespace mirror {
  class Array;
  class ClassLoader;
  class DexCache;
  template<class T> class ObjectArray;
  template<class T> class PrimitiveArray;
  typedef PrimitiveArray<int8_t> ByteArray;
  class String;
  class Throwable;
}  // namespace mirror
namespace ti {
  class Agent;
}  // namespace ti
namespace verifier {
  class MethodVerifier;
  enum class VerifyMode : int8_t;
}  // namespace verifier
class ArenaPool;
class ArtMethod;
class ClassHierarchyAnalysis;
class ClassLinker;
class Closure;
class CompilerCallbacks;
class DexFile;
class InternTable;
class JavaVMExt;
class LinearAlloc;
class MonitorList;
class MonitorPool;
class NullPointerHandler;
class OatFileManager;
class Plugin;
struct RuntimeArgumentMap;
class RuntimeCallbacks;
class SignalCatcher;
class StackOverflowHandler;
class SuspensionHandler;
class ThreadList;
class Trace;
struct TraceConfig;
class Transaction;

typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;

class Runtime {
 public:
  // Parse raw runtime options.
  static bool ParseOptions(const RuntimeOptions& raw_options,
                           bool ignore_unrecognized,
                           RuntimeArgumentMap* runtime_options);

  // Creates and initializes a new runtime.
  static bool Create(RuntimeArgumentMap&& runtime_options)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);

  // Creates and initializes a new runtime.
  static bool Create(const RuntimeOptions& raw_options, bool ignore_unrecognized)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);

  // IsAotCompiler for compilers that don't have a running runtime. Only dex2oat currently.
  bool IsAotCompiler() const {
    return !UseJitCompilation() && IsCompiler();
  }

  // IsCompiler is any runtime which has a running compiler, either dex2oat or JIT.
  bool IsCompiler() const {
    return compiler_callbacks_ != nullptr;
  }

  // If a compiler, are we compiling a boot image?
  bool IsCompilingBootImage() const;

  bool CanRelocate() const;

  bool ShouldRelocate() const {
    return must_relocate_ && CanRelocate();
  }

  bool MustRelocateIfPossible() const {
    return must_relocate_;
  }

  bool IsDex2OatEnabled() const {
    return dex2oat_enabled_ && IsImageDex2OatEnabled();
  }

  bool IsImageDex2OatEnabled() const {
    return image_dex2oat_enabled_;
  }

  CompilerCallbacks* GetCompilerCallbacks() {
    return compiler_callbacks_;
  }

  void SetCompilerCallbacks(CompilerCallbacks* callbacks) {
    CHECK(callbacks != nullptr);
    compiler_callbacks_ = callbacks;
  }

  bool IsZygote() const {
    return is_zygote_;
  }

  bool IsExplicitGcDisabled() const {
    return is_explicit_gc_disabled_;
  }

  std::string GetCompilerExecutable() const;
  std::string GetPatchoatExecutable() const;

  const std::vector<std::string>& GetCompilerOptions() const {
    return compiler_options_;
  }

  void AddCompilerOption(const std::string& option) {
    compiler_options_.push_back(option);
  }

  const std::vector<std::string>& GetImageCompilerOptions() const {
    return image_compiler_options_;
  }

  const std::string& GetImageLocation() const {
    return image_location_;
  }

  // Starts a runtime, which may cause threads to be started and code to run.
  bool Start() UNLOCK_FUNCTION(Locks::mutator_lock_);

  bool IsShuttingDown(Thread* self);
  bool IsShuttingDownLocked() const REQUIRES(Locks::runtime_shutdown_lock_) {
    return shutting_down_;
  }

  size_t NumberOfThreadsBeingBorn() const REQUIRES(Locks::runtime_shutdown_lock_) {
    return threads_being_born_;
  }

  void StartThreadBirth() REQUIRES(Locks::runtime_shutdown_lock_) {
    threads_being_born_++;
  }

  void EndThreadBirth() REQUIRES(Locks::runtime_shutdown_lock_);

  bool IsStarted() const {
    return started_;
  }

  bool IsFinishedStarting() const {
    return finished_starting_;
  }

  static Runtime* Current() {
    return instance_;
  }

  // Aborts semi-cleanly. Used in the implementation of LOG(FATAL), which most
  // callers should prefer.
  NO_RETURN static void Abort(const char* msg) REQUIRES(!Locks::abort_lock_);

  // Returns the "main" ThreadGroup, used when attaching user threads.
  jobject GetMainThreadGroup() const;

  // Returns the "system" ThreadGroup, used when attaching our internal threads.
  jobject GetSystemThreadGroup() const;

  // Returns the system ClassLoader which represents the CLASSPATH.
  jobject GetSystemClassLoader() const;

  // Attaches the calling native thread to the runtime.
  bool AttachCurrentThread(const char* thread_name, bool as_daemon, jobject thread_group,
                           bool create_peer);

  void CallExitHook(jint status);

  // Detaches the current native thread from the runtime.
  void DetachCurrentThread() REQUIRES(!Locks::mutator_lock_);

  void DumpDeoptimizations(std::ostream& os);
  void DumpForSigQuit(std::ostream& os);
  void DumpLockHolders(std::ostream& os);

  ~Runtime();

  const std::string& GetBootClassPathString() const {
    return boot_class_path_string_;
  }

  const std::string& GetClassPathString() const {
    return class_path_string_;
  }

  ClassLinker* GetClassLinker() const {
    return class_linker_;
  }

  size_t GetDefaultStackSize() const {
    return default_stack_size_;
  }

  gc::Heap* GetHeap() const {
    return heap_;
  }

  InternTable* GetInternTable() const {
    DCHECK(intern_table_ != nullptr);
    return intern_table_;
  }

  JavaVMExt* GetJavaVM() const {
    return java_vm_.get();
  }

  size_t GetMaxSpinsBeforeThinLockInflation() const {
    return max_spins_before_thin_lock_inflation_;
  }

  MonitorList* GetMonitorList() const {
    return monitor_list_;
  }

  MonitorPool* GetMonitorPool() const {
    return monitor_pool_;
  }

  // Is the given object the special object used to mark a cleared JNI weak global?
  bool IsClearedJniWeakGlobal(ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the special object used to mark a cleared JNI weak global.
  mirror::Object* GetClearedJniWeakGlobal() REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::Throwable* GetPreAllocatedOutOfMemoryError() REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::Throwable* GetPreAllocatedNoClassDefFoundError()
      REQUIRES_SHARED(Locks::mutator_lock_);

  const std::vector<std::string>& GetProperties() const {
    return properties_;
  }

  ThreadList* GetThreadList() const {
    return thread_list_;
  }

  static const char* GetVersion() {
    return "2.1.0";
  }

  bool IsMethodHandlesEnabled() const {
    return true;
  }

  void DisallowNewSystemWeaks() REQUIRES_SHARED(Locks::mutator_lock_);
  void AllowNewSystemWeaks() REQUIRES_SHARED(Locks::mutator_lock_);
  // broadcast_for_checkpoint is true when we broadcast for making blocking threads to respond to
  // checkpoint requests. It's false when we broadcast to unblock blocking threads after system weak
  // access is reenabled.
  void BroadcastForNewSystemWeaks(bool broadcast_for_checkpoint = false);

  // Visit all the roots. If only_dirty is true then non-dirty roots won't be visited. If
  // clean_dirty is true then dirty roots will be marked as non-dirty after visiting.
  void VisitRoots(RootVisitor* visitor, VisitRootFlags flags = kVisitRootFlagAllRoots)
      REQUIRES(!Locks::classlinker_classes_lock_, !Locks::trace_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit image roots, only used for hprof since the GC uses the image space mod union table
  // instead.
  void VisitImageRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all of the roots we can do safely do concurrently.
  void VisitConcurrentRoots(RootVisitor* visitor,
                            VisitRootFlags flags = kVisitRootFlagAllRoots)
      REQUIRES(!Locks::classlinker_classes_lock_, !Locks::trace_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all of the non thread roots, we can do this with mutators unpaused.
  void VisitNonThreadRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void VisitTransactionRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Flip thread roots from from-space refs to to-space refs.
  size_t FlipThreadRoots(Closure* thread_flip_visitor, Closure* flip_callback,
                         gc::collector::GarbageCollector* collector)
      REQUIRES(!Locks::mutator_lock_);

  // Sweep system weaks, the system weak is deleted if the visitor return null. Otherwise, the
  // system weak is updated to be the visitor's returned value.
  void SweepSystemWeaks(IsMarkedVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a special method that calls into a trampoline for runtime method resolution
  ArtMethod* GetResolutionMethod();

  bool HasResolutionMethod() const {
    return resolution_method_ != nullptr;
  }

  void SetResolutionMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);
  void ClearResolutionMethod() {
    resolution_method_ = nullptr;
  }

  ArtMethod* CreateResolutionMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a special method that calls into a trampoline for runtime imt conflicts.
  ArtMethod* GetImtConflictMethod();
  ArtMethod* GetImtUnimplementedMethod();

  bool HasImtConflictMethod() const {
    return imt_conflict_method_ != nullptr;
  }

  void ClearImtConflictMethod() {
    imt_conflict_method_ = nullptr;
  }

  void FixupConflictTables();
  void SetImtConflictMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);
  void SetImtUnimplementedMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* CreateImtConflictMethod(LinearAlloc* linear_alloc)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void ClearImtUnimplementedMethod() {
    imt_unimplemented_method_ = nullptr;
  }

  // Returns a special method that describes all callee saves being spilled to the stack.
  enum CalleeSaveType {
    kSaveAllCalleeSaves,  // All callee-save registers.
    kSaveRefsOnly,        // Only those callee-save registers that can hold references.
    kSaveRefsAndArgs,     // References (see above) and arguments (usually caller-save registers).
    kSaveEverything,      // All registers, including both callee-save and caller-save.
    kLastCalleeSaveType   // Value used for iteration
  };

  bool HasCalleeSaveMethod(CalleeSaveType type) const {
    return callee_save_methods_[type] != 0u;
  }

  ArtMethod* GetCalleeSaveMethod(CalleeSaveType type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* GetCalleeSaveMethodUnchecked(CalleeSaveType type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  QuickMethodFrameInfo GetCalleeSaveMethodFrameInfo(CalleeSaveType type) const {
    return callee_save_method_frame_infos_[type];
  }

  QuickMethodFrameInfo GetRuntimeMethodFrameInfo(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static size_t GetCalleeSaveMethodOffset(CalleeSaveType type) {
    return OFFSETOF_MEMBER(Runtime, callee_save_methods_[type]);
  }

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  void SetInstructionSet(InstructionSet instruction_set);
  void ClearInstructionSet();

  void SetCalleeSaveMethod(ArtMethod* method, CalleeSaveType type);
  void ClearCalleeSaveMethods();

  ArtMethod* CreateCalleeSaveMethod() REQUIRES_SHARED(Locks::mutator_lock_);

  int32_t GetStat(int kind);

  RuntimeStats* GetStats() {
    return &stats_;
  }

  bool HasStatsEnabled() const {
    return stats_enabled_;
  }

  void ResetStats(int kinds);

  void SetStatsEnabled(bool new_state)
      REQUIRES(!Locks::instrument_entrypoints_lock_, !Locks::mutator_lock_);

  enum class NativeBridgeAction {  // private
    kUnload,
    kInitialize
  };

  jit::Jit* GetJit() const {
    return jit_.get();
  }

  // Returns true if JIT compilations are enabled. GetJit() will be not null in this case.
  bool UseJitCompilation() const;

  void PreZygoteFork();
  void InitNonZygoteOrPostFork(
      JNIEnv* env, bool is_system_server, NativeBridgeAction action, const char* isa);

  const instrumentation::Instrumentation* GetInstrumentation() const {
    return &instrumentation_;
  }

  instrumentation::Instrumentation* GetInstrumentation() {
    return &instrumentation_;
  }

  void RegisterAppInfo(const std::vector<std::string>& code_paths,
                       const std::string& profile_output_filename);

  // Transaction support.
  bool IsActiveTransaction() const {
    return preinitialization_transaction_ != nullptr;
  }
  void EnterTransactionMode(Transaction* transaction);
  void ExitTransactionMode();
  bool IsTransactionAborted() const;

  void AbortTransactionAndThrowAbortError(Thread* self, const std::string& abort_message)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ThrowTransactionAbortError(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void RecordWriteFieldBoolean(mirror::Object* obj, MemberOffset field_offset, uint8_t value,
                               bool is_volatile) const;
  void RecordWriteFieldByte(mirror::Object* obj, MemberOffset field_offset, int8_t value,
                            bool is_volatile) const;
  void RecordWriteFieldChar(mirror::Object* obj, MemberOffset field_offset, uint16_t value,
                            bool is_volatile) const;
  void RecordWriteFieldShort(mirror::Object* obj, MemberOffset field_offset, int16_t value,
                          bool is_volatile) const;
  void RecordWriteField32(mirror::Object* obj, MemberOffset field_offset, uint32_t value,
                          bool is_volatile) const;
  void RecordWriteField64(mirror::Object* obj, MemberOffset field_offset, uint64_t value,
                          bool is_volatile) const;
  void RecordWriteFieldReference(mirror::Object* obj,
                                 MemberOffset field_offset,
                                 ObjPtr<mirror::Object> value,
                                 bool is_volatile) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  void RecordWriteArray(mirror::Array* array, size_t index, uint64_t value) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  void RecordStrongStringInsertion(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordWeakStringInsertion(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordStrongStringRemoval(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordWeakStringRemoval(ObjPtr<mirror::String> s) const
      REQUIRES(Locks::intern_table_lock_);
  void RecordResolveString(ObjPtr<mirror::DexCache> dex_cache, dex::StringIndex string_idx) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetFaultMessage(const std::string& message) REQUIRES(!fault_message_lock_);
  // Only read by the signal handler, NO_THREAD_SAFETY_ANALYSIS to prevent lock order violations
  // with the unexpected_signal_lock_.
  const std::string& GetFaultMessage() NO_THREAD_SAFETY_ANALYSIS {
    return fault_message_;
  }

  void AddCurrentRuntimeFeaturesAsDex2OatArguments(std::vector<std::string>* arg_vector) const;

  bool ExplicitStackOverflowChecks() const {
    return !implicit_so_checks_;
  }

  bool IsVerificationEnabled() const;
  bool IsVerificationSoftFail() const;

  bool IsDexFileFallbackEnabled() const {
    return allow_dex_file_fallback_;
  }

  const std::vector<std::string>& GetCpuAbilist() const {
    return cpu_abilist_;
  }

  bool IsRunningOnMemoryTool() const {
    return is_running_on_memory_tool_;
  }

  void SetTargetSdkVersion(int32_t version) {
    target_sdk_version_ = version;
  }

  int32_t GetTargetSdkVersion() const {
    return target_sdk_version_;
  }

  uint32_t GetZygoteMaxFailedBoots() const {
    return zygote_max_failed_boots_;
  }

  bool AreExperimentalFlagsEnabled(ExperimentalFlags flags) {
    return (experimental_flags_ & flags) != ExperimentalFlags::kNone;
  }

  // Create the JIT and instrumentation and code cache.
  void CreateJit();

  ArenaPool* GetArenaPool() {
    return arena_pool_.get();
  }
  ArenaPool* GetJitArenaPool() {
    return jit_arena_pool_.get();
  }
  const ArenaPool* GetArenaPool() const {
    return arena_pool_.get();
  }

  void ReclaimArenaPoolMemory();

  LinearAlloc* GetLinearAlloc() {
    return linear_alloc_.get();
  }

  jit::JitOptions* GetJITOptions() {
    return jit_options_.get();
  }

  bool IsJavaDebuggable() const {
    return is_java_debuggable_;
  }

  void SetJavaDebuggable(bool value);

  // Deoptimize the boot image, called for Java debuggable apps.
  void DeoptimizeBootImage();

  bool IsNativeDebuggable() const {
    return is_native_debuggable_;
  }

  void SetNativeDebuggable(bool value) {
    is_native_debuggable_ = value;
  }

  // Returns the build fingerprint, if set. Otherwise an empty string is returned.
  std::string GetFingerprint() {
    return fingerprint_;
  }

  // Called from class linker.
  void SetSentinel(mirror::Object* sentinel) REQUIRES_SHARED(Locks::mutator_lock_);

  // Create a normal LinearAlloc or low 4gb version if we are 64 bit AOT compiler.
  LinearAlloc* CreateLinearAlloc();

  OatFileManager& GetOatFileManager() const {
    DCHECK(oat_file_manager_ != nullptr);
    return *oat_file_manager_;
  }

  double GetHashTableMinLoadFactor() const;
  double GetHashTableMaxLoadFactor() const;

  void SetSafeMode(bool mode) {
    safe_mode_ = mode;
  }

  bool GetDumpNativeStackOnSigQuit() const {
    return dump_native_stack_on_sig_quit_;
  }

  bool GetPrunedDalvikCache() const {
    return pruned_dalvik_cache_;
  }

  void SetPrunedDalvikCache(bool pruned) {
    pruned_dalvik_cache_ = pruned;
  }

  void UpdateProcessState(ProcessState process_state);

  // Returns true if we currently care about long mutator pause.
  bool InJankPerceptibleProcessState() const {
    return process_state_ == kProcessStateJankPerceptible;
  }

  void RegisterSensitiveThread() const;

  void SetZygoteNoThreadSection(bool val) {
    zygote_no_threads_ = val;
  }

  bool IsZygoteNoThreadSection() const {
    return zygote_no_threads_;
  }

  // Returns if the code can be deoptimized asynchronously. Code may be compiled with some
  // optimization that makes it impossible to deoptimize.
  bool IsAsyncDeoptimizeable(uintptr_t code) const REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a saved copy of the environment (getenv/setenv values).
  // Used by Fork to protect against overwriting LD_LIBRARY_PATH, etc.
  char** GetEnvSnapshot() const {
    return env_snapshot_.GetSnapshot();
  }

  void AddSystemWeakHolder(gc::AbstractSystemWeakHolder* holder);
  void RemoveSystemWeakHolder(gc::AbstractSystemWeakHolder* holder);

  ClassHierarchyAnalysis* GetClassHierarchyAnalysis() {
    return cha_;
  }

  NO_RETURN
  static void Aborter(const char* abort_message);

  void AttachAgent(const std::string& agent_arg);

  const std::list<ti::Agent>& GetAgents() const {
    return agents_;
  }

  RuntimeCallbacks* GetRuntimeCallbacks();

  void InitThreadGroups(Thread* self);

  void SetDumpGCPerformanceOnShutdown(bool value) {
    dump_gc_performance_on_shutdown_ = value;
  }

  void IncrementDeoptimizationCount(DeoptimizationKind kind) {
    DCHECK_LE(kind, DeoptimizationKind::kLast);
    deoptimization_counts_[static_cast<size_t>(kind)]++;
  }

  uint32_t GetNumberOfDeoptimizations() const {
    uint32_t result = 0;
    for (size_t i = 0; i <= static_cast<size_t>(DeoptimizationKind::kLast); ++i) {
      result += deoptimization_counts_[i];
    }
    return result;
  }

 private:
  static void InitPlatformSignalHandlers();

  Runtime();

  void BlockSignals();

  bool Init(RuntimeArgumentMap&& runtime_options)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);
  void InitNativeMethods() REQUIRES(!Locks::mutator_lock_);
  void RegisterRuntimeNativeMethods(JNIEnv* env);

  void StartDaemonThreads();
  void StartSignalCatcher();

  void MaybeSaveJitProfilingInfo();

  // Visit all of the thread roots.
  void VisitThreadRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all other roots which must be done with mutators suspended.
  void VisitNonConcurrentRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Constant roots are the roots which never change after the runtime is initialized, they only
  // need to be visited once per GC cycle.
  void VisitConstantRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // A pointer to the active runtime or null.
  static Runtime* instance_;

  // NOTE: these must match the gc::ProcessState values as they come directly from the framework.
  static constexpr int kProfileForground = 0;
  static constexpr int kProfileBackground = 1;

  // 64 bit so that we can share the same asm offsets for both 32 and 64 bits.
  uint64_t callee_save_methods_[kLastCalleeSaveType];
  GcRoot<mirror::Throwable> pre_allocated_OutOfMemoryError_;
  GcRoot<mirror::Throwable> pre_allocated_NoClassDefFoundError_;
  ArtMethod* resolution_method_;
  ArtMethod* imt_conflict_method_;
  // Unresolved method has the same behavior as the conflict method, it is used by the class linker
  // for differentiating between unfilled imt slots vs conflict slots in superclasses.
  ArtMethod* imt_unimplemented_method_;

  // Special sentinel object used to invalid conditions in JNI (cleared weak references) and
  // JDWP (invalid references).
  GcRoot<mirror::Object> sentinel_;

  InstructionSet instruction_set_;
  QuickMethodFrameInfo callee_save_method_frame_infos_[kLastCalleeSaveType];

  CompilerCallbacks* compiler_callbacks_;
  bool is_zygote_;
  bool must_relocate_;
  bool is_concurrent_gc_enabled_;
  bool is_explicit_gc_disabled_;
  bool dex2oat_enabled_;
  bool image_dex2oat_enabled_;

  std::string compiler_executable_;
  std::string patchoat_executable_;
  std::vector<std::string> compiler_options_;
  std::vector<std::string> image_compiler_options_;
  std::string image_location_;

  std::string boot_class_path_string_;
  std::string class_path_string_;
  std::vector<std::string> properties_;

  std::list<ti::Agent> agents_;
  std::vector<Plugin> plugins_;

  // The default stack size for managed threads created by the runtime.
  size_t default_stack_size_;

  gc::Heap* heap_;

  std::unique_ptr<ArenaPool> jit_arena_pool_;
  std::unique_ptr<ArenaPool> arena_pool_;
  // Special low 4gb pool for compiler linear alloc. We need ArtFields to be in low 4gb if we are
  // compiling using a 32 bit image on a 64 bit compiler in case we resolve things in the image
  // since the field arrays are int arrays in this case.
  std::unique_ptr<ArenaPool> low_4gb_arena_pool_;

  // Shared linear alloc for now.
  std::unique_ptr<LinearAlloc> linear_alloc_;

  // The number of spins that are done before thread suspension is used to forcibly inflate.
  size_t max_spins_before_thin_lock_inflation_;
  MonitorList* monitor_list_;
  MonitorPool* monitor_pool_;

  ThreadList* thread_list_;

  InternTable* intern_table_;

  ClassLinker* class_linker_;

  SignalCatcher* signal_catcher_;
  std::string stack_trace_file_;

  std::unique_ptr<JavaVMExt> java_vm_;

  std::unique_ptr<jit::Jit> jit_;
  std::unique_ptr<jit::JitOptions> jit_options_;

  // Fault message, printed when we get a SIGSEGV.
  Mutex fault_message_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::string fault_message_ GUARDED_BY(fault_message_lock_);

  // A non-zero value indicates that a thread has been created but not yet initialized. Guarded by
  // the shutdown lock so that threads aren't born while we're shutting down.
  size_t threads_being_born_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  // Waited upon until no threads are being born.
  std::unique_ptr<ConditionVariable> shutdown_cond_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  // Set when runtime shutdown is past the point that new threads may attach.
  bool shutting_down_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  // The runtime is starting to shutdown but is blocked waiting on shutdown_cond_.
  bool shutting_down_started_ GUARDED_BY(Locks::runtime_shutdown_lock_);

  bool started_;

  // New flag added which tells us if the runtime has finished starting. If
  // this flag is set then the Daemon threads are created and the class loader
  // is created. This flag is needed for knowing if its safe to request CMS.
  bool finished_starting_;

  // Hooks supported by JNI_CreateJavaVM
  jint (*vfprintf_)(FILE* stream, const char* format, va_list ap);
  void (*exit_)(jint status);
  void (*abort_)();

  bool stats_enabled_;
  RuntimeStats stats_;

  const bool is_running_on_memory_tool_;

  std::unique_ptr<TraceConfig> trace_config_;

  instrumentation::Instrumentation instrumentation_;

  jobject main_thread_group_;
  jobject system_thread_group_;

  // As returned by ClassLoader.getSystemClassLoader().
  jobject system_class_loader_;

  // If true, then we dump the GC cumulative timings on shutdown.
  bool dump_gc_performance_on_shutdown_;

  // Transaction used for pre-initializing classes at compilation time.
  Transaction* preinitialization_transaction_;

  // If kNone, verification is disabled. kEnable by default.
  verifier::VerifyMode verify_;

  // If true, the runtime may use dex files directly with the interpreter if an oat file is not
  // available/usable.
  bool allow_dex_file_fallback_;

  // List of supported cpu abis.
  std::vector<std::string> cpu_abilist_;

  // Specifies target SDK version to allow workarounds for certain API levels.
  int32_t target_sdk_version_;

  // Implicit checks flags.
  bool implicit_null_checks_;       // NullPointer checks are implicit.
  bool implicit_so_checks_;         // StackOverflow checks are implicit.
  bool implicit_suspend_checks_;    // Thread suspension checks are implicit.

  // Whether or not the sig chain (and implicitly the fault handler) should be
  // disabled. Tools like dex2oat or patchoat don't need them. This enables
  // building a statically link version of dex2oat.
  bool no_sig_chain_;

  // Force the use of native bridge even if the app ISA matches the runtime ISA.
  bool force_native_bridge_;

  // Whether or not a native bridge has been loaded.
  //
  // The native bridge allows running native code compiled for a foreign ISA. The way it works is,
  // if standard dlopen fails to load native library associated with native activity, it calls to
  // the native bridge to load it and then gets the trampoline for the entry to native activity.
  //
  // The option 'native_bridge_library_filename' specifies the name of the native bridge.
  // When non-empty the native bridge will be loaded from the given file. An empty value means
  // that there's no native bridge.
  bool is_native_bridge_loaded_;

  // Whether we are running under native debugger.
  bool is_native_debuggable_;

  // Whether Java code needs to be debuggable.
  bool is_java_debuggable_;

  // The maximum number of failed boots we allow before pruning the dalvik cache
  // and trying again. This option is only inspected when we're running as a
  // zygote.
  uint32_t zygote_max_failed_boots_;

  // Enable experimental opcodes that aren't fully specified yet. The intent is to
  // eventually publish them as public-usable opcodes, but they aren't ready yet.
  //
  // Experimental opcodes should not be used by other production code.
  ExperimentalFlags experimental_flags_;

  // Contains the build fingerprint, if given as a parameter.
  std::string fingerprint_;

  // Oat file manager, keeps track of what oat files are open.
  OatFileManager* oat_file_manager_;

  // Whether or not we are on a low RAM device.
  bool is_low_memory_mode_;

  // Whether the application should run in safe mode, that is, interpreter only.
  bool safe_mode_;

  // Whether threads should dump their native stack on SIGQUIT.
  bool dump_native_stack_on_sig_quit_;

  // Whether the dalvik cache was pruned when initializing the runtime.
  bool pruned_dalvik_cache_;

  // Whether or not we currently care about pause times.
  ProcessState process_state_;

  // Whether zygote code is in a section that should not start threads.
  bool zygote_no_threads_;

  // Saved environment.
  class EnvSnapshot {
   public:
    EnvSnapshot() = default;
    void TakeSnapshot();
    char** GetSnapshot() const;

   private:
    std::unique_ptr<char*[]> c_env_vector_;
    std::vector<std::unique_ptr<std::string>> name_value_pairs_;

    DISALLOW_COPY_AND_ASSIGN(EnvSnapshot);
  } env_snapshot_;

  // Generic system-weak holders.
  std::vector<gc::AbstractSystemWeakHolder*> system_weak_holders_;

  ClassHierarchyAnalysis* cha_;

  std::unique_ptr<RuntimeCallbacks> callbacks_;

  std::atomic<uint32_t> deoptimization_counts_[
      static_cast<uint32_t>(DeoptimizationKind::kLast) + 1];

  DISALLOW_COPY_AND_ASSIGN(Runtime);
};
std::ostream& operator<<(std::ostream& os, const Runtime::CalleeSaveType& rhs);

}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_H_
