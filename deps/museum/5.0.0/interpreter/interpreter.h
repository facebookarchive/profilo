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

#ifndef ART_RUNTIME_INTERPRETER_INTERPRETER_H_
#define ART_RUNTIME_INTERPRETER_INTERPRETER_H_

#include "base/mutex.h"
#include "dex_file.h"

namespace art {
namespace mirror {
class ArtMethod;
class Object;
}  // namespace mirror

union JValue;
class MethodHelper;
class ShadowFrame;
class Thread;

namespace interpreter {

// Called by ArtMethod::Invoke, shadow frames arguments are taken from the args array.
extern void EnterInterpreterFromInvoke(Thread* self, mirror::ArtMethod* method,
                                       mirror::Object* receiver, uint32_t* args, JValue* result)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

extern void EnterInterpreterFromDeoptimize(Thread* self, ShadowFrame* shadow_frame,
                                           JValue* ret_val)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

extern JValue EnterInterpreterFromStub(Thread* self, MethodHelper& mh,
                                       const DexFile::CodeItem* code_item,
                                       ShadowFrame& shadow_frame)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

extern "C" void artInterpreterToInterpreterBridge(Thread* self, MethodHelper& mh,
                                                  const DexFile::CodeItem* code_item,
                                                  ShadowFrame* shadow_frame, JValue* result)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

}  // namespace interpreter

extern "C" void artInterpreterToCompiledCodeBridge(Thread* self, MethodHelper& mh,
                                                   const DexFile::CodeItem* code_item,
                                                   ShadowFrame* shadow_frame, JValue* result)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_H_
