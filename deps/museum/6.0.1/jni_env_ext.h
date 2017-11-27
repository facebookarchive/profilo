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
#include "reference_table.h"

namespace art {

class JavaVMExt;

// Maximum number of local references in the indirect reference table. The value is arbitrary but
// low enough that it forces sanity checks.
static constexpr size_t kLocalsMax = 512;

struct JNIEnvExt : public JNIEnv {
  static JNIEnvExt* Create(Thread* self, JavaVMExt* vm);

  ~JNIEnvExt();

  void DumpReferenceTables(std::ostream& os)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetCheckJniEnabled(bool enabled);

  void PushFrame(int capacity);
  void PopFrame();

  template<typename T>
  T AddLocalReference(mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static Offset SegmentStateOffset();

  static Offset LocalRefCookieOffset() {
    return Offset(OFFSETOF_MEMBER(JNIEnvExt, local_ref_cookie));
  }

  static Offset SelfOffset() {
    return Offset(OFFSETOF_MEMBER(JNIEnvExt, self));
  }

  jobject NewLocalRef(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DeleteLocalRef(jobject obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Thread* const self;
  JavaVMExt* const vm;

  // Cookie used when using the local indirect reference table.
  uint32_t local_ref_cookie;

  // JNI local references.
  IndirectReferenceTable locals GUARDED_BY(Locks::mutator_lock_);

  // Stack of cookies corresponding to PushLocalFrame/PopLocalFrame calls.
  // TODO: to avoid leaks (and bugs), we need to clear this vector on entry (or return)
  // to a native method.
  std::vector<uint32_t> stacked_local_ref_cookies;

  // Frequently-accessed fields cached from JavaVM.
  bool check_jni;

  // How many nested "critical" JNI calls are we in?
  int critical;

  // Entered JNI monitors, for bulk exit on thread detach.
  ReferenceTable monitors;

  // Used by -Xcheck:jni.
  const JNINativeInterface* unchecked_functions;

 private:
  // The constructor should not be called directly. It may leave the object in an erronuous state,
  // and the result needs to be checked.
  JNIEnvExt(Thread* self, JavaVMExt* vm);
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
  uint32_t saved_local_ref_cookie_;

  DISALLOW_COPY_AND_ASSIGN(ScopedJniEnvLocalRefState);
};

}  // namespace art

#endif  // ART_RUNTIME_JNI_ENV_EXT_H_
