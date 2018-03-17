/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_RUNTIME_CALLBACKS_H_
#define ART_RUNTIME_RUNTIME_CALLBACKS_H_

#include <vector>

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "handle.h"

namespace art {

namespace mirror {
class Class;
class ClassLoader;
}  // namespace mirror

class ArtMethod;
class ClassLoadCallback;
class Thread;
class MethodCallback;
class ThreadLifecycleCallback;

// Note: RuntimeCallbacks uses the mutator lock to synchronize the callback lists. A thread must
//       hold the exclusive lock to add or remove a listener. A thread must hold the shared lock
//       to dispatch an event. This setup is chosen as some clients may want to suspend the
//       dispatching thread or all threads.
//
//       To make this safe, the following restrictions apply:
//       * Only the owner of a listener may ever add or remove said listener.
//       * A listener must never add or remove itself or any other listener while running.
//       * It is the responsibility of the owner to not remove the listener while it is running
//         (and suspended).
//
//       The simplest way to satisfy these restrictions is to never remove a listener, and to do
//       any state checking (is the listener enabled) in the listener itself. For an example, see
//       Dbg.

class RuntimeSigQuitCallback {
 public:
  virtual ~RuntimeSigQuitCallback() {}

  virtual void SigQuit() REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

class RuntimePhaseCallback {
 public:
  enum RuntimePhase {
    kInitialAgents,   // Initial agent loading is done.
    kStart,           // The runtime is started.
    kInit,            // The runtime is initialized (and will run user code soon).
    kDeath,           // The runtime just died.
  };

  virtual ~RuntimePhaseCallback() {}

  virtual void NextRuntimePhase(RuntimePhase phase) REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

class RuntimeCallbacks {
 public:
  void AddThreadLifecycleCallback(ThreadLifecycleCallback* cb) REQUIRES(Locks::mutator_lock_);
  void RemoveThreadLifecycleCallback(ThreadLifecycleCallback* cb) REQUIRES(Locks::mutator_lock_);

  void ThreadStart(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void ThreadDeath(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);

  void AddClassLoadCallback(ClassLoadCallback* cb) REQUIRES(Locks::mutator_lock_);
  void RemoveClassLoadCallback(ClassLoadCallback* cb) REQUIRES(Locks::mutator_lock_);

  void ClassLoad(Handle<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  void ClassPrepare(Handle<mirror::Class> temp_klass, Handle<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AddRuntimeSigQuitCallback(RuntimeSigQuitCallback* cb)
      REQUIRES(Locks::mutator_lock_);
  void RemoveRuntimeSigQuitCallback(RuntimeSigQuitCallback* cb)
      REQUIRES(Locks::mutator_lock_);

  void SigQuit() REQUIRES_SHARED(Locks::mutator_lock_);

  void AddRuntimePhaseCallback(RuntimePhaseCallback* cb)
      REQUIRES(Locks::mutator_lock_);
  void RemoveRuntimePhaseCallback(RuntimePhaseCallback* cb)
      REQUIRES(Locks::mutator_lock_);

  void NextRuntimePhase(RuntimePhaseCallback::RuntimePhase phase)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void ClassPreDefine(const char* descriptor,
                      Handle<mirror::Class> temp_class,
                      Handle<mirror::ClassLoader> loader,
                      const DexFile& initial_dex_file,
                      const DexFile::ClassDef& initial_class_def,
                      /*out*/DexFile const** final_dex_file,
                      /*out*/DexFile::ClassDef const** final_class_def)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AddMethodCallback(MethodCallback* cb) REQUIRES(Locks::mutator_lock_);
  void RemoveMethodCallback(MethodCallback* cb) REQUIRES(Locks::mutator_lock_);

  void RegisterNativeMethod(ArtMethod* method,
                            const void* original_implementation,
                            /*out*/void** new_implementation)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  std::vector<ThreadLifecycleCallback*> thread_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<ClassLoadCallback*> class_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<RuntimeSigQuitCallback*> sigquit_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<RuntimePhaseCallback*> phase_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<MethodCallback*> method_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_CALLBACKS_H_
