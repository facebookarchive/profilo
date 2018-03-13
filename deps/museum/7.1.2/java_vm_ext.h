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

#ifndef ART_RUNTIME_JAVA_VM_EXT_H_
#define ART_RUNTIME_JAVA_VM_EXT_H_

#include "jni.h"

#include "base/macros.h"
#include "base/mutex.h"
#include "indirect_reference_table.h"
#include "reference_table.h"

namespace art {

namespace mirror {
  class Array;
}  // namespace mirror

class ArtMethod;
class Libraries;
class ParsedOptions;
class Runtime;
struct RuntimeArgumentMap;

class JavaVMExt : public JavaVM {
 public:
  JavaVMExt(Runtime* runtime, const RuntimeArgumentMap& runtime_options);
  ~JavaVMExt();

  bool ForceCopy() const {
    return force_copy_;
  }

  bool IsCheckJniEnabled() const {
    return check_jni_;
  }

  bool IsTracingEnabled() const {
    return tracing_enabled_;
  }

  Runtime* GetRuntime() const {
    return runtime_;
  }

  void SetCheckJniAbortHook(void (*hook)(void*, const std::string&), void* data) {
    check_jni_abort_hook_ = hook;
    check_jni_abort_hook_data_ = data;
  }

  // Aborts execution unless there is an abort handler installed in which case it will return. Its
  // therefore important that callers return after aborting as otherwise code following the abort
  // will be executed in the abort handler case.
  void JniAbort(const char* jni_function_name, const char* msg);

  void JniAbortV(const char* jni_function_name, const char* fmt, va_list ap);

  void JniAbortF(const char* jni_function_name, const char* fmt, ...)
      __attribute__((__format__(__printf__, 3, 4)));

  // If both "-Xcheck:jni" and "-Xjnitrace:" are enabled, we print trace messages
  // when a native method that matches the -Xjnitrace argument calls a JNI function
  // such as NewByteArray.
  // If -verbose:third-party-jni is on, we want to log any JNI function calls
  // made by a third-party native method.
  bool ShouldTrace(ArtMethod* method) SHARED_REQUIRES(Locks::mutator_lock_);

  /**
   * Loads the given shared library. 'path' is an absolute pathname.
   *
   * Returns 'true' on success. On failure, sets 'error_msg' to a
   * human-readable description of the error.
   */
  bool LoadNativeLibrary(JNIEnv* env,
                         const std::string& path,
                         jobject class_loader,
                         jstring library_path,
                         std::string* error_msg);

  // Unload native libraries with cleared class loaders.
  void UnloadNativeLibraries()
      REQUIRES(!Locks::jni_libraries_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  /**
   * Returns a pointer to the code for the native method 'm', found
   * using dlsym(3) on every native library that's been loaded so far.
   */
  void* FindCodeForNativeMethod(ArtMethod* m)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void DumpForSigQuit(std::ostream& os)
      REQUIRES(!Locks::jni_libraries_lock_, !globals_lock_, !weak_globals_lock_);

  void DumpReferenceTables(std::ostream& os)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!globals_lock_, !weak_globals_lock_);

  bool SetCheckJniEnabled(bool enabled);

  void VisitRoots(RootVisitor* visitor) SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!globals_lock_);

  void DisallowNewWeakGlobals() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!weak_globals_lock_);
  void AllowNewWeakGlobals() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!weak_globals_lock_);
  void BroadcastForNewWeakGlobals() SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!weak_globals_lock_);

  jobject AddGlobalRef(Thread* self, mirror::Object* obj)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!globals_lock_);

  jweak AddWeakGlobalRef(Thread* self, mirror::Object* obj)
    SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!weak_globals_lock_);

  void DeleteGlobalRef(Thread* self, jobject obj) REQUIRES(!globals_lock_);

  void DeleteWeakGlobalRef(Thread* self, jweak obj) REQUIRES(!weak_globals_lock_);

  void SweepJniWeakGlobals(IsMarkedVisitor* visitor)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!weak_globals_lock_);

  mirror::Object* DecodeGlobal(IndirectRef ref)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void UpdateGlobal(Thread* self, IndirectRef ref, mirror::Object* result)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!globals_lock_);

  mirror::Object* DecodeWeakGlobal(Thread* self, IndirectRef ref)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!weak_globals_lock_);

  mirror::Object* DecodeWeakGlobalLocked(Thread* self, IndirectRef ref)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(weak_globals_lock_);

  // Like DecodeWeakGlobal() but to be used only during a runtime shutdown where self may be
  // null.
  mirror::Object* DecodeWeakGlobalDuringShutdown(Thread* self, IndirectRef ref)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!weak_globals_lock_);

  // Checks if the weak global ref has been cleared by the GC without decode (read barrier.)
  bool IsWeakGlobalCleared(Thread* self, IndirectRef ref)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!weak_globals_lock_);

  Mutex& WeakGlobalsLock() RETURN_CAPABILITY(weak_globals_lock_) {
    return weak_globals_lock_;
  }

  void UpdateWeakGlobal(Thread* self, IndirectRef ref, mirror::Object* result)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!weak_globals_lock_);

  const JNIInvokeInterface* GetUncheckedFunctions() const {
    return unchecked_functions_;
  }

  void TrimGlobals() SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!globals_lock_);

 private:
  // Return true if self can currently access weak globals.
  bool MayAccessWeakGlobalsUnlocked(Thread* self) const SHARED_REQUIRES(Locks::mutator_lock_);
  bool MayAccessWeakGlobals(Thread* self) const
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(weak_globals_lock_);

  Runtime* const runtime_;

  // Used for testing. By default, we'll LOG(FATAL) the reason.
  void (*check_jni_abort_hook_)(void* data, const std::string& reason);
  void* check_jni_abort_hook_data_;

  // Extra checking.
  bool check_jni_;
  bool force_copy_;
  const bool tracing_enabled_;

  // Extra diagnostics.
  const std::string trace_;

  // JNI global references.
  ReaderWriterMutex globals_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  // Not guarded by globals_lock since we sometimes use SynchronizedGet in Thread::DecodeJObject.
  IndirectReferenceTable globals_;

  // No lock annotation since UnloadNativeLibraries is called on libraries_ but locks the
  // jni_libraries_lock_ internally.
  std::unique_ptr<Libraries> libraries_;

  // Used by -Xcheck:jni.
  const JNIInvokeInterface* const unchecked_functions_;

  // JNI weak global references.
  Mutex weak_globals_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  // Since weak_globals_ contain weak roots, be careful not to
  // directly access the object references in it. Use Get() with the
  // read barrier enabled.
  // Not guarded by weak_globals_lock since we may use SynchronizedGet in DecodeWeakGlobal.
  IndirectReferenceTable weak_globals_;
  // Not guarded by weak_globals_lock since we may use SynchronizedGet in DecodeWeakGlobal.
  Atomic<bool> allow_accessing_weak_globals_;
  ConditionVariable weak_globals_add_condition_ GUARDED_BY(weak_globals_lock_);

  DISALLOW_COPY_AND_ASSIGN(JavaVMExt);
};

}  // namespace art

#endif  // ART_RUNTIME_JAVA_VM_EXT_H_
