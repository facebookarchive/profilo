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

#ifndef ART_RUNTIME_JNI_ENV_EXT_H_
#define ART_RUNTIME_JNI_ENV_EXT_H_

#include <jni.h>

#include "base/macros.h"
#include "base/mutex.h"
#include "indirect_reference_table.h"
#include "object_callbacks.h"
#include "obj_ptr.h"
#include "reference_table.h"

namespace art {

class JavaVMExt;

namespace mirror {
class Object;
}  // namespace mirror

// Number of local references in the indirect reference table. The value is arbitrary but
// low enough that it forces sanity checks.
static constexpr size_t kLocalsInitial = 512;

struct JNIEnvExt : public JNIEnv {
  // Creates a new JNIEnvExt. Returns null on error, in which case error_msg
  // will contain a description of the error.
  static JNIEnvExt* Create(Thread* self, JavaVMExt* vm, std::string* error_msg);

  ~JNIEnvExt();

  void DumpReferenceTables(std::ostream& os)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetCheckJniEnabled(bool enabled) REQUIRES(!Locks::jni_function_table_lock_);

  void PushFrame(int capacity) REQUIRES_SHARED(Locks::mutator_lock_);
  void PopFrame() REQUIRES_SHARED(Locks::mutator_lock_);

  template<typename T>
  T AddLocalReference(ObjPtr<mirror::Object> obj) REQUIRES_SHARED(Locks::mutator_lock_);

  static Offset SegmentStateOffset(size_t pointer_size);
  static Offset LocalRefCookieOffset(size_t pointer_size);
  static Offset SelfOffset(size_t pointer_size);

  static jint GetEnvHandler(JavaVMExt* vm, /*out*/void** out, jint version);

  jobject NewLocalRef(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_);
  void DeleteLocalRef(jobject obj) REQUIRES_SHARED(Locks::mutator_lock_);

  Thread* const self;
  JavaVMExt* const vm;

  // Cookie used when using the local indirect reference table.
  IRTSegmentState local_ref_cookie;

  // JNI local references.
  IndirectReferenceTable locals GUARDED_BY(Locks::mutator_lock_);

  // Stack of cookies corresponding to PushLocalFrame/PopLocalFrame calls.
  // TODO: to avoid leaks (and bugs), we need to clear this vector on entry (or return)
  // to a native method.
  std::vector<IRTSegmentState> stacked_local_ref_cookies;

  // Frequently-accessed fields cached from JavaVM.
  bool check_jni;

  // If we are a JNI env for a daemon thread with a deleted runtime.
  bool runtime_deleted;

  // How many nested "critical" JNI calls are we in?
  int critical;

  // Entered JNI monitors, for bulk exit on thread detach.
  ReferenceTable monitors;

  // Used by -Xcheck:jni.
  const JNINativeInterface* unchecked_functions;

  // Functions to keep track of monitor lock and unlock operations. Used to ensure proper locking
  // rules in CheckJNI mode.

  // Record locking of a monitor.
  void RecordMonitorEnter(jobject obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Check the release, that is, that the release is performed in the same JNI "segment."
  void CheckMonitorRelease(jobject obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Check that no monitors are held that have been acquired in this JNI "segment."
  void CheckNoHeldMonitors() REQUIRES_SHARED(Locks::mutator_lock_);

  // Set the functions to the runtime shutdown functions.
  void SetFunctionsToRuntimeShutdownFunctions();

  // Set the function table override. This will install the override (or original table, if null)
  // to all threads.
  // Note: JNI function table overrides are sensitive to the order of operations wrt/ CheckJNI.
  //       After overriding the JNI function table, CheckJNI toggling is ignored.
  static void SetTableOverride(const JNINativeInterface* table_override)
      REQUIRES(!Locks::thread_list_lock_, !Locks::jni_function_table_lock_);

  // Return either the regular, or the CheckJNI function table. Will return table_override_ instead
  // if it is not null.
  static const JNINativeInterface* GetFunctionTable(bool check_jni)
      REQUIRES(Locks::jni_function_table_lock_);

 private:
  // Override of function tables. This applies to both default as well as instrumented (CheckJNI)
  // function tables.
  static const JNINativeInterface* table_override_ GUARDED_BY(Locks::jni_function_table_lock_);

  // The constructor should not be called directly. It may leave the object in an erroneous state,
  // and the result needs to be checked.
  JNIEnvExt(Thread* self, JavaVMExt* vm, std::string* error_msg)
      REQUIRES(!Locks::jni_function_table_lock_);

  // All locked objects, with the (Java caller) stack frame that locked them. Used in CheckJNI
  // to ensure that only monitors locked in this native frame are being unlocked, and that at
  // the end all are unlocked.
  std::vector<std::pair<uintptr_t, jobject>> locked_objects_;
};

// Used to save and restore the JNIEnvExt state when not going through code created by the JNI
// compiler.
class ScopedJniEnvLocalRefState {
 public:
  explicit ScopedJniEnvLocalRefState(JNIEnvExt* env) : env_(env) {
    saved_local_ref_cookie_ = env->local_ref_cookie;
    env->local_ref_cookie = env->locals.GetSegmentState();
  }

  ~ScopedJniEnvLocalRefState() {
    env_->locals.SetSegmentState(env_->local_ref_cookie);
    env_->local_ref_cookie = saved_local_ref_cookie_;
  }

 private:
  JNIEnvExt* const env_;
  IRTSegmentState saved_local_ref_cookie_;

  DISALLOW_COPY_AND_ASSIGN(ScopedJniEnvLocalRefState);
};

}  // namespace art

#endif  // ART_RUNTIME_JNI_ENV_EXT_H_
