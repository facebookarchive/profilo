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

#include <museum/6.0.1/libnativehelper/jni.h>
#include <museum/6.0.1/bionic/libc/stdio.h>

#include <museum/6.0.1/external/libcxx/iosfwd>
#include <museum/6.0.1/external/libcxx/set>
#include <museum/6.0.1/external/libcxx/string>
#include <museum/6.0.1/external/libcxx/utility>
#include <museum/6.0.1/external/libcxx/vector>

#include <museum/6.0.1/art/runtime/arch/instruction_set.h>
#include <museum/6.0.1/art/runtime/base/macros.h>
#include <museum/6.0.1/art/runtime/gc_root.h>
#include <museum/6.0.1/art/runtime/instrumentation.h>
#include <museum/6.0.1/art/runtime/jobject_comparator.h>
#include <museum/6.0.1/art/runtime/method_reference.h>
#include <museum/6.0.1/art/runtime/object_callbacks.h>
#include <museum/6.0.1/art/runtime/offsets.h>
#include <museum/6.0.1/art/runtime/profiler_options.h>
#include <museum/6.0.1/art/runtime/quick/quick_method_frame_info.h>
#include <museum/6.0.1/art/runtime/runtime_stats.h>
#include <museum/6.0.1/art/runtime/safe_map.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

namespace gc {
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
  class ClassLoader;
  class Array;
  template<class T> class ObjectArray;
  template<class T> class PrimitiveArray;
  typedef PrimitiveArray<int8_t> ByteArray;
  class String;
  class Throwable;
}  // namespace mirror
namespace verifier {
  class MethodVerifier;
}  // namespace verifier
class ArenaPool;
class ArtMethod;
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
class SignalCatcher;
class StackOverflowHandler;
class SuspensionHandler;
class ThreadList;
class Trace;
struct TraceConfig;
class Transaction;

typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;
typedef SafeMap<MethodReference, SafeMap<uint32_t, std::set<uint32_t>>,
    MethodReferenceComparator> MethodRefToStringInitRegMap;

// Not all combinations of flags are valid. You may not visit all roots as well as the new roots
// (no logical reason to do this). You also may not start logging new roots and stop logging new
// roots (also no logical reason to do this).
enum VisitRootFlags : uint8_t {
  kVisitRootFlagAllRoots = 0x1,
  kVisitRootFlagNewRoots = 0x2,
  kVisitRootFlagStartLoggingNewRoots = 0x4,
  kVisitRootFlagStopLoggingNewRoots = 0x8,
  kVisitRootFlagClearRootLog = 0x10,
  // Non moving means we can have optimizations where we don't visit some roots if they are
  // definitely reachable from another location. E.g. ArtMethod and ArtField roots.
  kVisitRootFlagNonMoving = 0x20,
};

class Runtime {
 public:
  // Creates and initializes a new runtime.
  static bool Create(const RuntimeOptions& options, bool ignore_unrecognized)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);

  // IsAotCompiler for compilers that don't have a running runtime. Only dex2oat currently.
  bool IsAotCompiler() const {
    return !UseJit() && IsCompiler();
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

  void AddCompilerOption(std::string option) {
    compiler_options_.push_back(option);
  }

  const std::vector<std::string>& GetImageCompilerOptions() const {
    return image_compiler_options_;
  }

  const std::string& GetImageLocation() const {
    return image_location_;
  }

  const ProfilerOptions& GetProfilerOptions() const {
    return profiler_options_;
  }

  // Starts a runtime, which may cause threads to be started and code to run.
  bool Start() UNLOCK_FUNCTION(Locks::mutator_lock_);

  bool IsShuttingDown(Thread* self);
  bool IsShuttingDownLocked() const EXCLUSIVE_LOCKS_REQUIRED(Locks::runtime_shutdown_lock_) {
    return shutting_down_;
  }

  size_t NumberOfThreadsBeingBorn() const EXCLUSIVE_LOCKS_REQUIRED(Locks::runtime_shutdown_lock_) {
    return threads_being_born_;
  }

  void StartThreadBirth() EXCLUSIVE_LOCKS_REQUIRED(Locks::runtime_shutdown_lock_) {
    threads_being_born_++;
  }

  void EndThreadBirth() EXCLUSIVE_LOCKS_REQUIRED(Locks::runtime_shutdown_lock_);

  bool IsStarted() const {
    return started_;
  }

  bool IsFinishedStarting() const {
    return finished_starting_;
  }

  static Runtime* Current() {
    return instance_();
  };

  // Aborts semi-cleanly. Used in the implementation of LOG(FATAL), which most
  // callers should prefer.
  NO_RETURN static void Abort() LOCKS_EXCLUDED(Locks::abort_lock_);

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
  void DetachCurrentThread() LOCKS_EXCLUDED(Locks::mutator_lock_);

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
    return java_vm_;
  }

  size_t GetMaxSpinsBeforeThinkLockInflation() const {
    return max_spins_before_thin_lock_inflation_;
  }

  MonitorList* GetMonitorList() const {
    return monitor_list_;
  }

  MonitorPool* GetMonitorPool() const {
    return monitor_pool_;
  }

  // Is the given object the special object used to mark a cleared JNI weak global?
  bool IsClearedJniWeakGlobal(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the special object used to mark a cleared JNI weak global.
  mirror::Object* GetClearedJniWeakGlobal() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Throwable* GetPreAllocatedOutOfMemoryError() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::Throwable* GetPreAllocatedNoClassDefFoundError()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const std::vector<std::string>& GetProperties() const {
    return properties_;
  }

  ThreadList* GetThreadList() const {
    return thread_list_;
  }

  static const char* GetVersion() {
    return "2.1.0";
  }

  void DisallowNewSystemWeaks() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void AllowNewSystemWeaks() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void EnsureNewSystemWeaksDisallowed() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Visit all the roots. If only_dirty is true then non-dirty roots won't be visited. If
  // clean_dirty is true then dirty roots will be marked as non-dirty after visiting.
  void VisitRoots(RootVisitor* visitor, VisitRootFlags flags = kVisitRootFlagAllRoots)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Visit image roots, only used for hprof since the GC uses the image space mod union table
  // instead.
  void VisitImageRoots(RootVisitor* visitor) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Visit all of the roots we can do safely do concurrently.
  void VisitConcurrentRoots(RootVisitor* visitor,
                            VisitRootFlags flags = kVisitRootFlagAllRoots)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Visit all of the non thread roots, we can do this with mutators unpaused.
  void VisitNonThreadRoots(RootVisitor* visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VisitTransactionRoots(RootVisitor* visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Visit all of the thread roots.
  void VisitThreadRoots(RootVisitor* visitor) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Flip thread roots from from-space refs to to-space refs.
  size_t FlipThreadRoots(Closure* thread_flip_visitor, Closure* flip_callback,
                         gc::collector::GarbageCollector* collector)
      LOCKS_EXCLUDED(Locks::mutator_lock_);

  // Visit all other roots which must be done with mutators suspended.
  void VisitNonConcurrentRoots(RootVisitor* visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Sweep system weaks, the system weak is deleted if the visitor return null. Otherwise, the
  // system weak is updated to be the visitor's returned value.
  void SweepSystemWeaks(IsMarkedCallback* visitor, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Constant roots are the roots which never change after the runtime is initialized, they only
  // need to be visited once per GC cycle.
  void VisitConstantRoots(RootVisitor* visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns a special method that calls into a trampoline for runtime method resolution
  ArtMethod* GetResolutionMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool HasResolutionMethod() const {
    return resolution_method_ != nullptr;
  }

  void SetResolutionMethod(ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* CreateResolutionMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns a special method that calls into a trampoline for runtime imt conflicts.
  ArtMethod* GetImtConflictMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ArtMethod* GetImtUnimplementedMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool HasImtConflictMethod() const {
    return imt_conflict_method_ != nullptr;
  }

  void SetImtConflictMethod(ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetImtUnimplementedMethod(ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* CreateImtConflictMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns a special method that describes all callee saves being spilled to the stack.
  enum CalleeSaveType {
    kSaveAll,
    kRefsOnly,
    kRefsAndArgs,
    kLastCalleeSaveType  // Value used for iteration
  };

  bool HasCalleeSaveMethod(CalleeSaveType type) const {
    return callee_save_methods_[type] != 0u;
  }

  ArtMethod* GetCalleeSaveMethod(CalleeSaveType type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetCalleeSaveMethodUnchecked(CalleeSaveType type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  QuickMethodFrameInfo GetCalleeSaveMethodFrameInfo(CalleeSaveType type) const {
    return callee_save_method_frame_infos_[type];
  }

  QuickMethodFrameInfo GetRuntimeMethodFrameInfo(ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static size_t GetCalleeSaveMethodOffset(CalleeSaveType type) {
    return OFFSETOF_MEMBER(Runtime, callee_save_methods_[type]);
  }

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  void SetInstructionSet(InstructionSet instruction_set);

  void SetCalleeSaveMethod(ArtMethod* method, CalleeSaveType type);

  ArtMethod* CreateCalleeSaveMethod() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t GetStat(int kind);

  RuntimeStats* GetStats() {
    return &stats_;
  }

  bool HasStatsEnabled() const {
    return stats_enabled_;
  }

  void ResetStats(int kinds);

  void SetStatsEnabled(bool new_state) LOCKS_EXCLUDED(Locks::instrument_entrypoints_lock_,
                                                      Locks::mutator_lock_);

  enum class NativeBridgeAction {  // private
    kUnload,
    kInitialize
  };

  jit::Jit* GetJit() {
    return jit_.get();
  }
  bool UseJit() const {
    return jit_.get() != nullptr;
  }

  void PreZygoteFork();
  bool InitZygote();
  void DidForkFromZygote(JNIEnv* env, NativeBridgeAction action, const char* isa);

  const instrumentation::Instrumentation* GetInstrumentation() const {
    return &instrumentation_;
  }

  instrumentation::Instrumentation* GetInstrumentation() {
    return &instrumentation_;
  }

  void StartProfiler(const char* profile_output_filename);
  void UpdateProfilerState(int state);

  // Transaction support.
  bool IsActiveTransaction() const {
    return preinitialization_transaction_ != nullptr;
  }
  void EnterTransactionMode(Transaction* transaction);
  void ExitTransactionMode();
  bool IsTransactionAborted() const;

  void AbortTransactionAndThrowAbortError(Thread* self, const std::string& abort_message)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void ThrowTransactionAbortError(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

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
  void RecordWriteFieldReference(mirror::Object* obj, MemberOffset field_offset,
                                 mirror::Object* value, bool is_volatile) const;
  void RecordWriteArray(mirror::Array* array, size_t index, uint64_t value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void RecordStrongStringInsertion(mirror::String* s) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RecordWeakStringInsertion(mirror::String* s) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RecordStrongStringRemoval(mirror::String* s) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);
  void RecordWeakStringRemoval(mirror::String* s) const
      EXCLUSIVE_LOCKS_REQUIRED(Locks::intern_table_lock_);

  void SetFaultMessage(const std::string& message);
  // Only read by the signal handler, NO_THREAD_SAFETY_ANALYSIS to prevent lock order violations
  // with the unexpected_signal_lock_.
  const std::string& GetFaultMessage() NO_THREAD_SAFETY_ANALYSIS {
    return fault_message_;
  }

  void AddCurrentRuntimeFeaturesAsDex2OatArguments(std::vector<std::string>* arg_vector) const;

  bool ExplicitStackOverflowChecks() const {
    return !implicit_so_checks_;
  }

  bool IsVerificationEnabled() const {
    return verify_;
  }

  bool IsDexFileFallbackEnabled() const {
    return allow_dex_file_fallback_;
  }

  const std::vector<std::string>& GetCpuAbilist() const {
    return cpu_abilist_;
  }

  bool RunningOnValgrind() const {
    return running_on_valgrind_;
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

  // Create the JIT and instrumentation and code cache.
  void CreateJit();

  ArenaPool* GetArenaPool() {
    return arena_pool_.get();
  }
  const ArenaPool* GetArenaPool() const {
    return arena_pool_.get();
  }
  LinearAlloc* GetLinearAlloc() {
    return linear_alloc_.get();
  }

  jit::JitOptions* GetJITOptions() {
    return jit_options_.get();
  }

  MethodRefToStringInitRegMap& GetStringInitMap() {
    return method_ref_string_init_reg_map_;
  }

  // Returns the build fingerprint, if set. Otherwise an empty string is returned.
  std::string GetFingerprint() {
    return fingerprint_;
  }

 private:
  static void InitPlatformSignalHandlers();

  Runtime();

  void BlockSignals();

  bool Init(const RuntimeOptions& options, bool ignore_unrecognized)
      SHARED_TRYLOCK_FUNCTION(true, Locks::mutator_lock_);
  void InitNativeMethods() LOCKS_EXCLUDED(Locks::mutator_lock_);
  void InitThreadGroups(Thread* self);
  void RegisterRuntimeNativeMethods(JNIEnv* env);

  void StartDaemonThreads();
  void StartSignalCatcher();

  // A pointer to the active runtime or null.
  static Runtime*& instance_();

  // NOTE: these must match the gc::ProcessState values as they come directly from the framework.
  static constexpr int kProfileForground = 0;
  static constexpr int kProfileBackgrouud = 1;

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

  // The default stack size for managed threads created by the runtime.
  size_t default_stack_size_;

  gc::Heap* heap_;

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

  JavaVMExt* java_vm_;

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

  const bool running_on_valgrind_;

  std::string profile_output_filename_;
  ProfilerOptions profiler_options_;
  bool profiler_started_;

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

  // If false, verification is disabled. True by default.
  bool verify_;

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

  // The maximum number of failed boots we allow before pruning the dalvik cache
  // and trying again. This option is only inspected when we're running as a
  // zygote.
  uint32_t zygote_max_failed_boots_;

  MethodRefToStringInitRegMap method_ref_string_init_reg_map_;

  // Contains the build fingerprint, if given as a parameter.
  std::string fingerprint_;

  DISALLOW_COPY_AND_ASSIGN(Runtime);
};
std::ostream& operator<<(std::ostream& os, const Runtime::CalleeSaveType& rhs);

} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_RUNTIME_H_
