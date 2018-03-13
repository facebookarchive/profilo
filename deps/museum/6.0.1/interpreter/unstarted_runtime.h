/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_H_
#define ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_H_

#include "interpreter.h"

#include "dex_file.h"
#include "jvalue.h"

namespace art {

class ArtMethod;
class Thread;
class ShadowFrame;

namespace mirror {
class Object;
}  // namespace mirror

namespace interpreter {

// Support for an unstarted runtime. These are special handwritten implementations for select
// libcore native and non-native methods so we can compile-time initialize classes in the boot
// image.
//
// While it would technically be OK to only expose the public functions, a class was chosen to
// wrap this so the actual implementations are exposed for testing. This is also why the private
// methods are not documented here - they are not intended to be used directly except in
// testing.

class UnstartedRuntime {
 public:
  static void Initialize();

  static void Invoke(Thread* self,
                     const DexFile::CodeItem* code_item,
                     ShadowFrame* shadow_frame,
                     JValue* result,
                     size_t arg_offset)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void Jni(Thread* self,
                  ArtMethod* method,
                  mirror::Object* receiver,
                  uint32_t* args,
                  JValue* result)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Methods that intercept available libcore implementations.
#define UNSTARTED_DIRECT(ShortName, SigIgnored)                 \
  static void Unstarted ## ShortName(Thread* self,              \
                                     ShadowFrame* shadow_frame, \
                                     JValue* result,            \
                                     size_t arg_offset)         \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
#include "unstarted_runtime_list.h"
  UNSTARTED_RUNTIME_DIRECT_LIST(UNSTARTED_DIRECT)
#undef UNSTARTED_RUNTIME_DIRECT_LIST
#undef UNSTARTED_RUNTIME_JNI_LIST
#undef UNSTARTED_DIRECT

  // Methods that are native.
#define UNSTARTED_JNI(ShortName, SigIgnored)                       \
  static void UnstartedJNI ## ShortName(Thread* self,              \
                                        ArtMethod* method, \
                                        mirror::Object* receiver,  \
                                        uint32_t* args,            \
                                        JValue* result)            \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
#include "unstarted_runtime_list.h"
  UNSTARTED_RUNTIME_JNI_LIST(UNSTARTED_JNI)
#undef UNSTARTED_RUNTIME_DIRECT_LIST
#undef UNSTARTED_RUNTIME_JNI_LIST
#undef UNSTARTED_JNI

  static void InitializeInvokeHandlers();
  static void InitializeJNIHandlers();

  friend class UnstartedRuntimeTest;

  DISALLOW_ALLOCATION();
  DISALLOW_COPY_AND_ASSIGN(UnstartedRuntime);
};

}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_H_
