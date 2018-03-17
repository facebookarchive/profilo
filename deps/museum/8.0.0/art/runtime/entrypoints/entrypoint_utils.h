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

#ifndef ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
#define ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_

#include <jni.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_instruction.h"
#include "dex_file_types.h"
#include "gc/allocator_type.h"
#include "handle.h"
#include "invoke_type.h"
#include "jvalue.h"
#include "runtime.h"

namespace art {

namespace mirror {
  class Array;
  class Class;
  class Object;
  class String;
}  // namespace mirror

class ArtField;
class ArtMethod;
class OatQuickMethodHeader;
class ScopedObjectAccessAlreadyRunnable;
class Thread;

// Given the context of a calling Method, use its DexCache to resolve a type to a Class. If it
// cannot be resolved, throw an error. If it can, use it to create an instance.
template <bool kInstrumented>
ALWAYS_INLINE inline mirror::Object* AllocObjectFromCode(mirror::Class* klass,
                                                         Thread* self,
                                                         gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Given the context of a calling Method and a resolved class, create an instance.
template <bool kInstrumented>
ALWAYS_INLINE inline mirror::Object* AllocObjectFromCodeResolved(mirror::Class* klass,
                                                                 Thread* self,
                                                                 gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Given the context of a calling Method and an initialized class, create an instance.
template <bool kInstrumented>
ALWAYS_INLINE inline mirror::Object* AllocObjectFromCodeInitialized(
    mirror::Class* klass,
    Thread* self,
    gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);


template <bool kAccessCheck>
ALWAYS_INLINE inline mirror::Class* CheckArrayAlloc(dex::TypeIndex type_idx,
                                                    int32_t component_count,
                                                    ArtMethod* method,
                                                    bool* slow_path)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Given the context of a calling Method, use its DexCache to resolve a type to an array Class. If
// it cannot be resolved, throw an error. If it can, use it to create an array.
// When verification/compiler hasn't been able to verify access, optionally perform an access
// check.
template <bool kAccessCheck, bool kInstrumented>
ALWAYS_INLINE inline mirror::Array* AllocArrayFromCode(dex::TypeIndex type_idx,
                                                       int32_t component_count,
                                                       ArtMethod* method,
                                                       Thread* self,
                                                       gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

template <bool kInstrumented>
ALWAYS_INLINE inline mirror::Array* AllocArrayFromCodeResolved(mirror::Class* klass,
                                                               int32_t component_count,
                                                               Thread* self,
                                                               gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Type of find field operation for fast and slow case.
enum FindFieldType {
  InstanceObjectRead,
  InstanceObjectWrite,
  InstancePrimitiveRead,
  InstancePrimitiveWrite,
  StaticObjectRead,
  StaticObjectWrite,
  StaticPrimitiveRead,
  StaticPrimitiveWrite,
};

template<FindFieldType type, bool access_check>
inline ArtField* FindFieldFromCode(uint32_t field_idx,
                                   ArtMethod* referrer,
                                   Thread* self,
                                   size_t expected_size)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

template<InvokeType type, bool access_check>
inline ArtMethod* FindMethodFromCode(uint32_t method_idx,
                                     ObjPtr<mirror::Object>* this_object,
                                     ArtMethod* referrer,
                                     Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Fast path field resolution that can't initialize classes or throw exceptions.
inline ArtField* FindFieldFast(uint32_t field_idx,
                               ArtMethod* referrer,
                               FindFieldType type,
                               size_t expected_size)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Fast path method resolution that can't throw exceptions.
inline ArtMethod* FindMethodFast(uint32_t method_idx,
                                 ObjPtr<mirror::Object> this_object,
                                 ArtMethod* referrer,
                                 bool access_check,
                                 InvokeType type)
    REQUIRES_SHARED(Locks::mutator_lock_);

inline mirror::Class* ResolveVerifyAndClinit(dex::TypeIndex type_idx,
                                             ArtMethod* referrer,
                                             Thread* self,
                                             bool can_run_clinit,
                                             bool verify_access)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

inline mirror::String* ResolveStringFromCode(ArtMethod* referrer, dex::StringIndex string_idx)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// TODO: annotalysis disabled as monitor semantics are maintained in Java code.
inline void UnlockJniSynchronizedMethod(jobject locked, Thread* self)
    NO_THREAD_SAFETY_ANALYSIS REQUIRES(!Roles::uninterruptible_);

void CheckReferenceResult(Handle<mirror::Object> o, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

JValue InvokeProxyInvocationHandler(ScopedObjectAccessAlreadyRunnable& soa, const char* shorty,
                                    jobject rcvr_jobj, jobject interface_art_method_jobj,
                                    std::vector<jvalue>& args)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

bool FillArrayData(ObjPtr<mirror::Object> obj, const Instruction::ArrayDataPayload* payload)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

template <typename INT_TYPE, typename FLOAT_TYPE>
inline INT_TYPE art_float_to_integral(FLOAT_TYPE f);

ArtMethod* GetCalleeSaveMethodCaller(ArtMethod** sp,
                                     Runtime::CalleeSaveType type,
                                     bool do_caller_check = false)
    REQUIRES_SHARED(Locks::mutator_lock_);

struct CallerAndOuterMethod {
  ArtMethod* caller;
  ArtMethod* outer_method;
};

CallerAndOuterMethod GetCalleeSaveMethodCallerAndOuterMethod(Thread* self,
                                                             Runtime::CalleeSaveType type)
    REQUIRES_SHARED(Locks::mutator_lock_);

ArtMethod* GetCalleeSaveOuterMethod(Thread* self, Runtime::CalleeSaveType type)
    REQUIRES_SHARED(Locks::mutator_lock_);

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
