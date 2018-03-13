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
#include "gc/allocator_type.h"
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

template <const bool kAccessCheck>
ALWAYS_INLINE inline mirror::Class* CheckObjectAlloc(uint32_t type_idx,
                                                     ArtMethod* method,
                                                     Thread* self, bool* slow_path)
    SHARED_REQUIRES(Locks::mutator_lock_);

ALWAYS_INLINE inline mirror::Class* CheckClassInitializedForObjectAlloc(mirror::Class* klass,
                                                                        Thread* self,
                                                                        bool* slow_path)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Given the context of a calling Method, use its DexCache to resolve a type to a Class. If it
// cannot be resolved, throw an error. If it can, use it to create an instance.
// When verification/compiler hasn't been able to verify access, optionally perform an access
// check.
template <bool kAccessCheck, bool kInstrumented>
ALWAYS_INLINE inline mirror::Object* AllocObjectFromCode(uint32_t type_idx,
                                                         ArtMethod* method,
                                                         Thread* self,
                                                         gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Given the context of a calling Method and a resolved class, create an instance.
template <bool kInstrumented>
ALWAYS_INLINE inline mirror::Object* AllocObjectFromCodeResolved(mirror::Class* klass,
                                                                 Thread* self,
                                                                 gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Given the context of a calling Method and an initialized class, create an instance.
template <bool kInstrumented>
ALWAYS_INLINE inline mirror::Object* AllocObjectFromCodeInitialized(mirror::Class* klass,
                                                                    Thread* self,
                                                                    gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);


template <bool kAccessCheck>
ALWAYS_INLINE inline mirror::Class* CheckArrayAlloc(uint32_t type_idx,
                                                    int32_t component_count,
                                                    ArtMethod* method,
                                                    bool* slow_path)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Given the context of a calling Method, use its DexCache to resolve a type to an array Class. If
// it cannot be resolved, throw an error. If it can, use it to create an array.
// When verification/compiler hasn't been able to verify access, optionally perform an access
// check.
template <bool kAccessCheck, bool kInstrumented>
ALWAYS_INLINE inline mirror::Array* AllocArrayFromCode(uint32_t type_idx,
                                                       int32_t component_count,
                                                       ArtMethod* method,
                                                       Thread* self,
                                                       gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);

template <bool kAccessCheck, bool kInstrumented>
ALWAYS_INLINE inline mirror::Array* AllocArrayFromCodeResolved(mirror::Class* klass,
                                                               int32_t component_count,
                                                               ArtMethod* method,
                                                               Thread* self,
                                                               gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);

extern mirror::Array* CheckAndAllocArrayFromCode(uint32_t type_idx, int32_t component_count,
                                                 ArtMethod* method, Thread* self,
                                                 bool access_check,
                                                 gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);

extern mirror::Array* CheckAndAllocArrayFromCodeInstrumented(uint32_t type_idx,
                                                             int32_t component_count,
                                                             ArtMethod* method,
                                                             Thread* self,
                                                             bool access_check,
                                                             gc::AllocatorType allocator_type)
    SHARED_REQUIRES(Locks::mutator_lock_);

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
inline ArtField* FindFieldFromCode(
    uint32_t field_idx, ArtMethod* referrer, Thread* self, size_t expected_size)
    SHARED_REQUIRES(Locks::mutator_lock_);

template<InvokeType type, bool access_check>
inline ArtMethod* FindMethodFromCode(
    uint32_t method_idx, mirror::Object** this_object, ArtMethod* referrer, Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Fast path field resolution that can't initialize classes or throw exceptions.
inline ArtField* FindFieldFast(
    uint32_t field_idx, ArtMethod* referrer, FindFieldType type, size_t expected_size)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Fast path method resolution that can't throw exceptions.
inline ArtMethod* FindMethodFast(
    uint32_t method_idx, mirror::Object* this_object, ArtMethod* referrer, bool access_check,
    InvokeType type)
    SHARED_REQUIRES(Locks::mutator_lock_);

inline mirror::Class* ResolveVerifyAndClinit(
    uint32_t type_idx, ArtMethod* referrer, Thread* self, bool can_run_clinit, bool verify_access)
    SHARED_REQUIRES(Locks::mutator_lock_);

inline mirror::String* ResolveStringFromCode(ArtMethod* referrer, uint32_t string_idx)
    SHARED_REQUIRES(Locks::mutator_lock_);

// TODO: annotalysis disabled as monitor semantics are maintained in Java code.
inline void UnlockJniSynchronizedMethod(jobject locked, Thread* self)
    NO_THREAD_SAFETY_ANALYSIS;

void CheckReferenceResult(mirror::Object* o, Thread* self)
    SHARED_REQUIRES(Locks::mutator_lock_);

JValue InvokeProxyInvocationHandler(ScopedObjectAccessAlreadyRunnable& soa, const char* shorty,
                                    jobject rcvr_jobj, jobject interface_art_method_jobj,
                                    std::vector<jvalue>& args)
    SHARED_REQUIRES(Locks::mutator_lock_);

bool FillArrayData(mirror::Object* obj, const Instruction::ArrayDataPayload* payload)
    SHARED_REQUIRES(Locks::mutator_lock_);

template <typename INT_TYPE, typename FLOAT_TYPE>
inline INT_TYPE art_float_to_integral(FLOAT_TYPE f);

ArtMethod* GetCalleeSaveMethodCaller(ArtMethod** sp,
                                     Runtime::CalleeSaveType type,
                                     bool do_caller_check = false);

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
